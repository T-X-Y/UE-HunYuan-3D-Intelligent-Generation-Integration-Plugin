// SAIChatWindowMultiView.cpp
// 多视图：多视图UI构建、图片上传、提交

#include "CoreMinimal.h"
#include "UI/SAIChatWindow.h"
#include "API/HunYuanAPITypes.h"

// Slate 核心组件
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Images/SImage.h"
#include "Styling/CoreStyle.h"

// 图片处理
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Engine/Texture2D.h"

// 桌面平台功能
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"

// 文件系统
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"

// JSON
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "SAIChatWindow"

// ==================== 图片缓存管理器 ====================

class FImageCache
{
private:
    struct FCachedImage
    {
        UTexture2D* Texture = nullptr;
        FString Base64Data;
        FDateTime LastAccess;
        int32 Width = 0;
        int32 Height = 0;
        int64 FileSize = 0;
        bool bIsValid = false;
    };

    TMap<FString, FCachedImage> Cache;
    FCriticalSection CacheCriticalSection;

    // 缓存有效期（10分钟）
    static constexpr double CacheValidityMinutes = 10.0;

public:
    ~FImageCache()
    {
        Clear();
    }

    // 尝试从缓存获取
    bool TryGet(const FString& Path, UTexture2D*& OutTexture, FString& OutBase64,
        int32& OutWidth, int32& OutHeight, int64& OutFileSize)
    {
        FScopeLock Lock(&CacheCriticalSection);

        if (FCachedImage* Cached = Cache.Find(Path))
        {
            if (Cached->bIsValid && Cached->Texture)
            {
                OutTexture = Cached->Texture;
                OutBase64 = Cached->Base64Data;
                OutWidth = Cached->Width;
                OutHeight = Cached->Height;
                OutFileSize = Cached->FileSize;
                Cached->LastAccess = FDateTime::Now();

                UE_LOG(LogTemp, Log, TEXT("图片缓存命中: %s"), *FPaths::GetCleanFilename(Path));
                return true;
            }
        }

        return false;
    }

    // 添加到缓存
    void Add(const FString& Path, UTexture2D* Texture, const FString& Base64Data,
        int32 Width, int32 Height, int64 FileSize)
    {
        FScopeLock Lock(&CacheCriticalSection);

        // 清理过期缓存
        CleanupExpired();

        FCachedImage NewCache;
        NewCache.Texture = Texture;
        NewCache.Base64Data = Base64Data;
        NewCache.Width = Width;
        NewCache.Height = Height;
        NewCache.FileSize = FileSize;
        NewCache.LastAccess = FDateTime::Now();
        NewCache.bIsValid = true;

        // 如果已存在，先移除旧的
        if (FCachedImage* Existing = Cache.Find(Path))
        {
            if (Existing->Texture && Existing->Texture != Texture)
            {
                // 注意：不要删除纹理，让GC处理
            }
        }

        Cache.Add(Path, NewCache);
        UE_LOG(LogTemp, Log, TEXT("图片添加到缓存: %s"), *FPaths::GetCleanFilename(Path));
    }

    // 清除指定缓存
    void Remove(const FString& Path)
    {
        FScopeLock Lock(&CacheCriticalSection);
        Cache.Remove(Path);
    }

    // 清空所有缓存
    void Clear()
    {
        FScopeLock Lock(&CacheCriticalSection);
        Cache.Empty();
    }

private:
    // 清理过期缓存
    void CleanupExpired()
    {
        FDateTime Now = FDateTime::Now();
        TArray<FString> ToRemove;

        for (auto& Pair : Cache)
        {
            FTimespan Age = Now - Pair.Value.LastAccess;
            if (Age.GetTotalMinutes() > CacheValidityMinutes)
            {
                ToRemove.Add(Pair.Key);
                UE_LOG(LogTemp, Verbose, TEXT("清理过期缓存: %s"), *FPaths::GetCleanFilename(Pair.Key));
            }
        }

        for (const FString& Key : ToRemove)
        {
            Cache.Remove(Key);
        }
    }
};

// 全局缓存实例
static FImageCache GImageCache;

// ==================== 异步图片加载任务 ====================

class FAsyncImageLoadTask : public FNonAbandonableTask
{
    friend class FAutoDeleteAsyncTask<FAsyncImageLoadTask>;

    FString ImagePath;
    EViewType ViewType;
    TWeakPtr<SAIChatWindow> WindowPtr;
    TSharedPtr<FAsyncImageLoadResult> Result;

public:
    FAsyncImageLoadTask(const FString& InPath, EViewType InType, TSharedPtr<SAIChatWindow> InWindow)
        : ImagePath(InPath), ViewType(InType), WindowPtr(InWindow)
    {
        Result = MakeShared<FAsyncImageLoadResult>();
        Result->FilePath = InPath;
        Result->FileName = FPaths::GetCleanFilename(InPath);
        Result->RawData = MakeShared<TArray<uint8>>();
    }

    void DoWork()
    {
        UE_LOG(LogTemp, Log, TEXT("[异步加载] 开始加载图片: %s"), *Result->FileName);

        TArray<uint8> FileData;
        if (!FFileHelper::LoadFileToArray(FileData, *ImagePath))
        {
            UE_LOG(LogTemp, Error, TEXT("[异步加载] 读取文件失败: %s"), *Result->FileName);
            Result->bSuccess = false;
            OnLoadComplete();
            return;
        }

        Result->FileSize = FileData.Num();
        Result->Base64Data = FBase64::Encode(FileData);

        IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
        EImageFormat Format = ImageWrapperModule.DetectImageFormat(FileData.GetData(), FileData.Num());

        if (Format == EImageFormat::Invalid)
        {
            UE_LOG(LogTemp, Error, TEXT("[异步加载] 不支持的图片格式: %s"), *Result->FileName);
            Result->bSuccess = false;
            OnLoadComplete();
            return;
        }

        TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(Format);
        if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
        {
            UE_LOG(LogTemp, Error, TEXT("[异步加载] 创建ImageWrapper失败: %s"), *Result->FileName);
            Result->bSuccess = false;
            OnLoadComplete();
            return;
        }

        // 获取原始尺寸
        int32 OriginalWidth = ImageWrapper->GetWidth();
        int32 OriginalHeight = ImageWrapper->GetHeight();

        // 获取完整的原始数据
        TArray<uint8> RawData;
        if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
        {
            UE_LOG(LogTemp, Error, TEXT("[异步加载] 获取原始数据失败: %s"), *Result->FileName);
            Result->bSuccess = false;
            OnLoadComplete();
            return;
        }

        Result->Width = OriginalWidth;
        Result->Height = OriginalHeight;
        *Result->RawData = MoveTemp(RawData);

        // ========== 生成缩略图 ==========
        const int32 ThumbnailSize = 256;
        if (OriginalWidth > ThumbnailSize || OriginalHeight > ThumbnailSize)
        {
            // 计算缩略图尺寸
            float Scale = FMath::Min((float)ThumbnailSize / OriginalWidth,
                (float)ThumbnailSize / OriginalHeight);
            int32 ThumbWidth = FMath::RoundToInt(OriginalWidth * Scale);
            int32 ThumbHeight = FMath::RoundToInt(OriginalHeight * Scale);

            // 创建缩略图数据数组
            TArray<uint8> ThumbData;
            ThumbData.SetNum(ThumbWidth * ThumbHeight * 4);

            // 手动降采样生成缩略图（使用简单的最近邻采样，速度快）
            for (int32 y = 0; y < ThumbHeight; y++)
            {
                for (int32 x = 0; x < ThumbWidth; x++)
                {
                    // 计算源图像中的对应坐标
                    int32 SrcX = (int32)((float)x / ThumbWidth * OriginalWidth);
                    int32 SrcY = (int32)((float)y / ThumbHeight * OriginalHeight);
                    SrcX = FMath::Clamp(SrcX, 0, OriginalWidth - 1);
                    SrcY = FMath::Clamp(SrcY, 0, OriginalHeight - 1);

                    // 复制像素
                    for (int32 c = 0; c < 4; c++)
                    {
                        ThumbData[(y * ThumbWidth + x) * 4 + c] =
                            (*Result->RawData)[(SrcY * OriginalWidth + SrcX) * 4 + c];
                    }
                }
            }

            Result->ThumbnailData = MakeShared<TArray<uint8>>(MoveTemp(ThumbData));
            Result->ThumbnailWidth = ThumbWidth;
            Result->ThumbnailHeight = ThumbHeight;

            UE_LOG(LogTemp, Log, TEXT("[异步加载] 生成缩略图: %dx%d -> %dx%d, 数据大小=%d"),
                OriginalWidth, OriginalHeight, ThumbWidth, ThumbHeight, Result->ThumbnailData->Num());
        }
        else
        {
            // 图片本身不大，直接用原图作为缩略图
            Result->ThumbnailData = Result->RawData;
            Result->ThumbnailWidth = Result->Width;
            Result->ThumbnailHeight = Result->Height;

            UE_LOG(LogTemp, Log, TEXT("[异步加载] 图片较小，直接使用原图作为缩略图: %dx%d"),
                Result->Width, Result->Height);
        }

        Result->bSuccess = true;
        UE_LOG(LogTemp, Log, TEXT("[异步加载] 加载完成: %s, 原图=%dx%d (%.2f MB), 缩略图=%dx%d (%.2f KB)"),
            *Result->FileName, Result->Width, Result->Height,
            Result->FileSize / (1024.0f * 1024.0f),
            Result->ThumbnailWidth, Result->ThumbnailHeight,
            Result->ThumbnailData->Num() / 1024.0f);

        OnLoadComplete();
    }

    void OnLoadComplete()
    {
        // 设置 ViewType
        Result->ViewType = ViewType;

        // 回到游戏线程更新UI
        TSharedPtr<SAIChatWindow> Window = WindowPtr.Pin();
        if (Window.IsValid())
        {
            // 修复：使用正确的捕获方式
            FAsyncImageLoadResult* HeapResult = new FAsyncImageLoadResult(*Result);

            AsyncTask(ENamedThreads::GameThread, [Window, HeapResult]()
                {
                    if (Window.IsValid())
                    {
                        Window->OnAsyncImageLoaded(HeapResult->ViewType, *HeapResult);
                    }
                    delete HeapResult;
                });
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[异步加载] 窗口已销毁，放弃加载结果"));
        }
    }

    FORCEINLINE TStatId GetStatId() const
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncImageLoadTask, STATGROUP_ThreadPoolAsyncTasks);
    }
};

// ==================== 多视图UI构建 ====================

TSharedRef<SWidget> SAIChatWindow::BuildMultiViewUploadSection()
{
    // 定义视图顺序
    TArray<EViewType> ViewOrder = {
        EViewType::Front,
        EViewType::Left45,
        EViewType::Left,
        EViewType::Back,
        EViewType::Right,
        EViewType::Right45,
        EViewType::Top,
        EViewType::Bottom
    };

    // 视图名称映射
    TMap<EViewType, FString> ViewNames;
    ViewNames.Add(EViewType::Front, TEXT("正视图"));
    ViewNames.Add(EViewType::Back, TEXT("背视图"));
    ViewNames.Add(EViewType::Left, TEXT("左视图"));
    ViewNames.Add(EViewType::Right, TEXT("右视图"));
    ViewNames.Add(EViewType::Top, TEXT("顶视图"));
    ViewNames.Add(EViewType::Bottom, TEXT("底视图"));
    ViewNames.Add(EViewType::Left45, TEXT("左45°"));
    ViewNames.Add(EViewType::Right45, TEXT("右45°"));

    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
        .BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f))
        .Padding(10)
        [
            SNew(SVerticalBox)

                // 标题和模式开关
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 10)
                [
                    SNew(SHorizontalBox)

                        // 标题
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Text(LOCTEXT("MultiViewUpload", "多视图图片上传"))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                        ]
                ]

            // 提示信息
            + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 10)
                [
                    SNew(STextBlock)
                        .Text(LOCTEXT("MultiViewHint",
                            "提示：最少上传1张正视图，最多8张（正、背、左、右、顶、底、左45°、右45°）\n"
                            "上传更多视角可提升模型生成质量和完整性"))
                        .ColorAndOpacity(FLinearColor::Yellow)
                        .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                        .AutoWrapText(true)
                ]

            // 视图网格
            + SVerticalBox::Slot()
             .AutoHeight()
            [
                SNew(SScrollBox)
                    .Orientation(Orient_Horizontal)
                    .ScrollBarVisibility(EVisibility::Visible)
                    .ConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible)
                    + SScrollBox::Slot()
                    [
                        CreateViewSlots(ViewOrder, ViewNames)
                    ]
            ]

                // 已上传图片计数
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 10, 0, 0)
                [
                    SNew(STextBlock)
                        .Text_Lambda([this]()
                            {
                                return FText::Format(
                                    LOCTEXT("UploadCount", "已上传 {0}/8 张图片"),
                                    FText::AsNumber(GetValidViewCount()));
                            })
                        .ColorAndOpacity(FLinearColor::Gray)
                ]
        ];
}

TSharedRef<SWidget> SAIChatWindow::CreateViewSlots(
    const TArray<EViewType>& ViewOrder,
    const TMap<EViewType, FString>& ViewNames)
{
    // 使用水平盒子而不是网格，方便滚动
    TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

    const float SlotWidth = 160.0f;  // 固定宽度

    for (const EViewType& ViewType : ViewOrder)
    {
        const FString& ViewName = ViewNames[ViewType];

        // 确保 Map 中有这个键的条目，避免断言失败
        if (!ViewImages.Contains(ViewType))
        {
            FViewImageItem NewItem;
            NewItem.ViewType = ViewType;
            ViewImages.Add(ViewType, NewItem);
        }

        if (!ViewClearButtons.Contains(ViewType))
        {
            ViewClearButtons.Add(ViewType, nullptr);
        }

        if (!ViewInfoTexts.Contains(ViewType))
        {
            ViewInfoTexts.Add(ViewType, nullptr);
        }

        // 创建图片控件并存储
        TSharedPtr<SImage> ImageWidget;

        // 修改3：固定宽度，避免被挤压
        HorizontalBox->AddSlot()
            .AutoWidth()
            .Padding(5)
            [
                SNew(SBox)
                    .WidthOverride(SlotWidth)
                    [
                        SNew(SBorder)
                            .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                            .BorderBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f, 1.0f))
                            .Padding(5)
                            [
                                SNew(SVerticalBox)

                                    // 视图标题
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .HAlign(HAlign_Center)
                                    .Padding(0, 0, 0, 5)
                                    [
                                        SNew(STextBlock)
                                            .Text(FText::FromString(ViewName))
                                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                                    ]

                                    // 图片预览区域
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .HAlign(HAlign_Center)
                                    [
                                        SNew(SBox)
                                            .WidthOverride(120)
                                            .HeightOverride(120)
                                            [
                                                SAssignNew(ViewImages[ViewType].ImageWidget, SImage)
                                                    .Image(FCoreStyle::Get().GetDefaultBrush())
                                            ]
                                    ]

                                // 按钮区域
                                + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .HAlign(HAlign_Center)
                                    .Padding(0, 5, 0, 0)
                                    [
                                        SNew(SHorizontalBox)

                                            + SHorizontalBox::Slot()
                                            .AutoWidth()
                                            .Padding(2)
                                            [
                                                SNew(SButton)
                                                    .Text(LOCTEXT("Upload", "上传"))
                                                    .OnClicked(this, &SAIChatWindow::OnUploadViewImageClicked, ViewType)
                                            ]

                                            + SHorizontalBox::Slot()
                                            .AutoWidth()
                                            .Padding(2)
                                            [
                                                SAssignNew(ViewClearButtons[ViewType], SButton)
                                                    .Text(LOCTEXT("Clear", "清除"))
                                                    .OnClicked(this, &SAIChatWindow::OnClearViewImageClicked, ViewType)
                                                    .IsEnabled(false)
                                            ]
                                    ]

                                // 图片信息
                                + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .HAlign(HAlign_Center)
                                    .Padding(0, 5, 0, 0)
                                    [
                                        SAssignNew(ViewInfoTexts[ViewType], STextBlock)
                                            .Text(LOCTEXT("NoImage", "未上传"))
                                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                                            .ColorAndOpacity(FLinearColor::Gray)
                                    ]
                            ]
                    ]
            ];
    }

    return HorizontalBox;
}

// ==================== 多视图图片上传 ====================

FReply SAIChatWindow::OnUploadViewImageClicked(EViewType ViewType)
{
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (DesktopPlatform)
    {
        TArray<FString> OutFiles;

        FString DialogTitle = FString::Printf(TEXT("上传%s图片"), *GetViewUploadButtonText(ViewType).ToString());
        FString Filter = TEXT("图片文件|*.jpg;*.jpeg;*.png;*.bmp|所有文件|*.*");

        if (DesktopPlatform->OpenFileDialog(
            FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
            DialogTitle,
            FPaths::ProjectContentDir(),
            TEXT(""),
            Filter,
            EFileDialogFlags::None,
            OutFiles) && OutFiles.Num() > 0)
        {
            // 立即更新UI状态为"加载中"
            SetViewImageLoadingState(ViewType, true);

            // 先尝试从缓存获取（使用原图路径）
            UTexture2D* CachedTexture = nullptr;
            FString CachedBase64;
            int32 CachedWidth = 0, CachedHeight = 0;
            int64 CachedFileSize = 0;

            if (GImageCache.TryGet(OutFiles[0], CachedTexture, CachedBase64,
                CachedWidth, CachedHeight, CachedFileSize))
            {
                // 缓存命中，直接使用
                UE_LOG(LogTemp, Log, TEXT("使用缓存图片: %s"), *FPaths::GetCleanFilename(OutFiles[0]));
                UpdateViewImagePreview(ViewType, CachedTexture, CachedBase64,
                    FPaths::GetCleanFilename(OutFiles[0]), CachedFileSize);
                SetViewImageLoadingState(ViewType, false);
            }
            else
            {
                // 缓存未命中，异步加载
                UE_LOG(LogTemp, Log, TEXT("异步加载图片: %s"), *FPaths::GetCleanFilename(OutFiles[0]));
                (new FAutoDeleteAsyncTask<FAsyncImageLoadTask>(OutFiles[0], ViewType, SharedThis(this)))
                    ->StartBackgroundTask();
            }
        }
    }
    return FReply::Handled();
}
void SAIChatWindow::SetViewImageLoadingState(EViewType ViewType, bool bIsLoading)
{
    if (ViewInfoTexts.Contains(ViewType))
    {
        if (bIsLoading)
        {
            ViewInfoTexts[ViewType]->SetText(LOCTEXT("Loading", "加载中..."));
            ViewInfoTexts[ViewType]->SetColorAndOpacity(FLinearColor::Yellow);
        }
        else
        {
            ViewInfoTexts[ViewType]->SetColorAndOpacity(FLinearColor::Gray);
        }
    }

    if (ViewClearButtons.Contains(ViewType))
    {
        ViewClearButtons[ViewType]->SetEnabled(!bIsLoading);
    }
}

void SAIChatWindow::OnAsyncImageLoaded(EViewType ViewType, const FAsyncImageLoadResult& Result)
{
    UE_LOG(LogTemp, Warning, TEXT("=== OnAsyncImageLoaded 被调用 ==="));
    UE_LOG(LogTemp, Warning, TEXT("ViewType=%d, FileName=%s, bSuccess=%d, Base64长度=%d"),
        (int32)ViewType, *Result.FileName, Result.bSuccess, Result.Base64Data.Len());

    // 检查缩略图数据
    if (Result.ThumbnailData.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("缩略图数据有效，大小=%d"), Result.ThumbnailData->Num());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("缩略图数据无效！"));
    }

    // 确保在游戏线程执行
    if (!IsInGameThread())
    {
        UE_LOG(LogTemp, Warning, TEXT("OnAsyncImageLoaded: 不在游戏线程，调度到游戏线程执行"));

        FAsyncImageLoadResult* HeapResult = new FAsyncImageLoadResult(Result);
        HeapResult->ViewType = ViewType;

        TSharedPtr<SAIChatWindow> SharedThis = StaticCastSharedRef<SAIChatWindow>(AsShared());

        AsyncTask(ENamedThreads::GameThread, [SharedThis, HeapResult]()
            {
                if (SharedThis.IsValid())
                {
                    SharedThis->OnAsyncImageLoaded(HeapResult->ViewType, *HeapResult);
                }
                delete HeapResult;
            });
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("OnAsyncImageLoaded: ViewType=%d, FileName=%s, bSuccess=%d, 原图=%dx%d, 缩略图=%dx%d"),
        (int32)ViewType, *Result.FileName, Result.bSuccess,
        Result.Width, Result.Height, Result.ThumbnailWidth, Result.ThumbnailHeight);

    if (!Result.bSuccess)
    {
        FString ErrorMsg = FString::Printf(TEXT("加载图片失败: %s"), *Result.FileName);
        AddErrorMessage(ErrorMsg);
        SetViewImageLoadingState(ViewType, false);
        return;
    }

    if (!Result.ThumbnailData.IsValid() || Result.ThumbnailData->Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("OnAsyncImageLoaded: 缩略图数据无效 - %s"), *Result.FileName);
        SetViewImageLoadingState(ViewType, false);
        return;
    }

    // 1. 立即创建并显示缩略图（快速响应，不卡顿）
    UTexture2D* ThumbnailTexture = Result.CreateTexture(true);

    if (ThumbnailTexture)
    {
        UE_LOG(LogTemp, Log, TEXT("OnAsyncImageLoaded: 缩略图显示完成: %s, 尺寸=%dx%d"),
            *Result.FileName, Result.ThumbnailWidth, Result.ThumbnailHeight);

        // 立即显示缩略图，同时保存原图的 Base64 数据
        // 注意：这里传入的是 Result.Base64Data（原图的 Base64），不是缩略图的
        UpdateViewImagePreview(ViewType, ThumbnailTexture, Result.Base64Data,
            Result.FileName, Result.FileSize);

        // 重要：确保 ViewImages 中已经保存了 Base64 数据
        if (ViewImages.Contains(ViewType))
        {
            UE_LOG(LogTemp, Log, TEXT("OnAsyncImageLoaded: 已保存 Base64 数据到 ViewType %d, 长度=%d"), 
                (int32)ViewType, ViewImages[ViewType].Base64Data.Len());
        }

        // 2. 如果原图大于缩略图，在后台加载原图纹理（用于更好的显示质量）
        // 注意：Base64 数据已经保存，后台只需加载原图纹理用于显示替换
        if (Result.Width > Result.ThumbnailWidth || Result.Height > Result.ThumbnailHeight)
        {
            UE_LOG(LogTemp, Log, TEXT("OnAsyncImageLoaded: 原图较大(%.1f MB)，后台加载原图纹理用于显示"),
                Result.FileSize / (1024.0f * 1024.0f));

            TWeakPtr<SAIChatWindow> WeakWindowPtr = StaticCastSharedRef<SAIChatWindow>(AsShared());
            FAsyncImageLoadResult* HeapResult = new FAsyncImageLoadResult(Result);

            // 延迟加载原图，避免卡顿
            FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateLambda([WeakWindowPtr, HeapResult, ViewType](float DeltaTime) -> bool
                    {
                        TSharedPtr<SAIChatWindow> Window = WeakWindowPtr.Pin();
                        if (Window.IsValid())
                        {
                            // 创建原图纹理（用于更好的显示质量）
                            UTexture2D* FullTexture = HeapResult->CreateTexture(false);
                            if (FullTexture)
                            {
                                // 更新显示为原图（替换缩略图）
                                if (Window->ViewImages.Contains(ViewType) && 
                                    Window->ViewImages[ViewType].ImageWidget.IsValid())
                                {
                                    // 更新画笔为原图
                                    if (!Window->ViewImages[ViewType].Brush.IsValid())
                                    {
                                        Window->ViewImages[ViewType].Brush = MakeShareable(new FSlateBrush());
                                        Window->ViewImages[ViewType].Brush->DrawAs = ESlateBrushDrawType::Image;
                                    }
                                    
                                    Window->ViewImages[ViewType].Brush->SetResourceObject(FullTexture);
                                    Window->ViewImages[ViewType].Brush->ImageSize = FVector2D(FullTexture->GetSizeX(), FullTexture->GetSizeY());
                                    Window->ViewImages[ViewType].ImageWidget->SetImage(Window->ViewImages[ViewType].Brush.Get());
                                    
                                    // 更新缓存为原图
                                    GImageCache.Add(HeapResult->FilePath, FullTexture, HeapResult->Base64Data,
                                        HeapResult->Width, HeapResult->Height, HeapResult->FileSize);
                                    
                                    // 标记已加载原图纹理
                                    Window->ViewImages[ViewType].bHasFullTexture = true;
                                }

                                UE_LOG(LogTemp, Log, TEXT("后台原图纹理加载完成: %s, 尺寸=%dx%d"),
                                    *HeapResult->FileName, HeapResult->Width, HeapResult->Height);
                            }
                        }
                        delete HeapResult;
                        return false;
                    }),
                0.2f
            );
        }
        else
        {
            // 原图不大，直接缓存原图
            GImageCache.Add(Result.FilePath, ThumbnailTexture, Result.Base64Data,
                Result.Width, Result.Height, Result.FileSize);
        }

        SetViewImageLoadingState(ViewType, false);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("OnAsyncImageLoaded: 创建缩略图失败 - %s"), *Result.FileName);
        SetViewImageLoadingState(ViewType, false);
    }
}

FReply SAIChatWindow::OnClearViewImageClicked(EViewType ViewType)
{
    if (ViewImages.Contains(ViewType))
    {
        FViewImageItem& Item = ViewImages[ViewType];

        // 清除缓存
        if (!Item.FilePath.IsEmpty())
        {
            GImageCache.Remove(Item.FilePath);
        }

        // 释放纹理资源
        if (Item.Brush.IsValid())
        {
            UObject* Resource = Item.Brush->GetResourceObject();
            if (Resource && IsValid(Resource))
            {
                Resource->RemoveFromRoot();
            }
            Item.Brush->SetResourceObject(nullptr);
        }

        Item.FilePath.Empty();
        Item.bIsLoaded = false;
        Item.Base64Data.Empty();

        if (Item.ImageWidget.IsValid())
        {
            Item.ImageWidget->SetImage(FCoreStyle::Get().GetDefaultBrush());
        }

        if (ViewInfoTexts.Contains(ViewType))
        {
            ViewInfoTexts[ViewType]->SetText(LOCTEXT("NoImage", "未上传"));
            ViewInfoTexts[ViewType]->SetColorAndOpacity(FLinearColor::Gray);
        }

        if (ViewClearButtons.Contains(ViewType))
        {
            ViewClearButtons[ViewType]->SetEnabled(false);
        }
    }
    return FReply::Handled();
}

FText SAIChatWindow::GetViewUploadButtonText(EViewType ViewType) const
{
    switch (ViewType)
    {
    case EViewType::Front:   return LOCTEXT("Front", "正视图");
    case EViewType::Back:    return LOCTEXT("Back", "背视图");
    case EViewType::Left:    return LOCTEXT("Left", "左视图");
    case EViewType::Right:   return LOCTEXT("Right", "右视图");
    case EViewType::Top:     return LOCTEXT("Top", "顶视图");
    case EViewType::Bottom:  return LOCTEXT("Bottom", "底视图");
    case EViewType::Left45:  return LOCTEXT("Left45", "左45°");
    case EViewType::Right45: return LOCTEXT("Right45", "右45°");
    default:                 return LOCTEXT("Unknown", "未知");
    }
}

int32 SAIChatWindow::GetValidViewCount() const
{
    int32 Count = 0;
    for (const auto& Pair : ViewImages)
    {
        if (Pair.Value.bIsLoaded)
        {
            Count++;
        }
    }
    return Count;
}

bool SAIChatWindow::IsViewImageValid(EViewType ViewType) const
{
    return ViewImages.Contains(ViewType) && ViewImages[ViewType].bIsLoaded;
}

void SAIChatWindow::HandleViewImageUploaded(EViewType ViewType, const FString& ImagePath)
{
    if (LoadViewImagePreview(ViewType, ImagePath))
    {
        FViewImageItem& Item = ViewImages[ViewType];
        Item.FilePath = ImagePath;
        Item.bIsLoaded = true;

        // 存储 Base64 数据供提交使用
        TArray<uint8> FileData;
        if (FFileHelper::LoadFileToArray(FileData, *ImagePath))
        {
            Item.Base64Data = FBase64::Encode(FileData);
        }

        // 更新信息文本
        FString FileName = FPaths::GetCleanFilename(ImagePath);
        int64 FileSize = IFileManager::Get().FileSize(*ImagePath);

        if (ViewInfoTexts.Contains(ViewType))
        {
            FString SizeString = FString::Printf(TEXT("%.1f KB"), FileSize / 1024.0f);
            ViewInfoTexts[ViewType]->SetText(FText::Format(
                LOCTEXT("ImageLoaded", "{0}\n{1}"),
                FText::FromString(FileName),
                FText::FromString(SizeString)));
        }

        if (ViewClearButtons.Contains(ViewType))
        {
            ViewClearButtons[ViewType]->SetEnabled(true);
        }

        AddSystemMessage(FString::Printf(
            TEXT("已上传%s: %s"),
            *GetViewUploadButtonText(ViewType).ToString(),
            *FileName));
    }
    else
    {
        AddErrorMessage(FString::Printf(
            TEXT("加载%s图片失败"),
            *GetViewUploadButtonText(ViewType).ToString()));
    }
}

bool SAIChatWindow::LoadViewImagePreview(EViewType ViewType, const FString& ImagePath)
{
    // 读取文件数据
    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *ImagePath))
    {
        return false;
    }

    // 获取 ImageWrapper 模块
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

    // 检测图片格式
    EImageFormat Format = ImageWrapperModule.DetectImageFormat(FileData.GetData(), FileData.Num());
    if (Format == EImageFormat::Invalid)
    {
        return false;
    }

    // 创建 ImageWrapper
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(Format);
    if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
    {
        return false;
    }

    // 获取原始数据
    TArray<uint8> RawData;
    if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
    {
        return false;
    }

    // 创建纹理
    UTexture2D* Texture = UTexture2D::CreateTransient(
        ImageWrapper->GetWidth(),
        ImageWrapper->GetHeight(),
        PF_B8G8R8A8);

    if (!Texture)
    {
        return false;
    }

    // 填充纹理数据
    void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(TextureData, RawData.GetData(), RawData.Num());
    Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
    Texture->UpdateResource();

    // 创建或更新 Slate 画笔
    if (!ViewImages[ViewType].Brush.IsValid())
    {
        ViewImages[ViewType].Brush = MakeShareable(new FSlateBrush());
        ViewImages[ViewType].Brush->DrawAs = ESlateBrushDrawType::Image;
    }

    ViewImages[ViewType].Brush->SetResourceObject(Texture);
    ViewImages[ViewType].Brush->ImageSize = FVector2D(ImageWrapper->GetWidth(), ImageWrapper->GetHeight());

    if (ViewImages[ViewType].ImageWidget.IsValid())
    {
        ViewImages[ViewType].ImageWidget->SetImage(ViewImages[ViewType].Brush.Get());
    }

    return true;
}

// ==================== 多视图提交 ====================

void SAIChatWindow::SubmitMultiViewTo3D(const HunYuanAPI::FMultiViewInput& MultiViewInput)
{
    UE_LOG(LogTemp, Warning, TEXT("=== 开始提交多视图任务 ==="));

    // 分离主视图（正视图）和其他视图
    HunYuanAPI::FMultiViewImage FrontImage;
    bool bHasFrontImage = false;
    TArray<HunYuanAPI::FMultiViewImage> OtherViews;

    for (const auto& Image : MultiViewInput.Images)
    {
        if (!Image.bIsValid || Image.Base64Data.IsEmpty())
        {
            continue;
        }

        // 正视图作为主图
        if (Image.ViewType == EViewType::Front)
        {
            FrontImage = Image;
            bHasFrontImage = true;
            UE_LOG(LogTemp, Log, TEXT("找到正视图，Base64长度: %d"), Image.Base64Data.Len());
        }
        else
        {
            // 其他7个视角（back、left、right、top、bottom、left45、right45）
            OtherViews.Add(Image);
            UE_LOG(LogTemp, Log, TEXT("添加其他视角: ViewType=%s, Base64长度=%d"),
                *Image.GetViewTypeString(), Image.Base64Data.Len());
        }
    }

    // 验证必须有正视图
    if (!bHasFrontImage)
    {
        AddErrorMessage(TEXT("多视图模式必须包含正视图"));
        UE_LOG(LogTemp, Error, TEXT("提交失败：没有正视图"));
        return;
    }

    int32 TotalImages = 1 + OtherViews.Num();
    UE_LOG(LogTemp, Warning, TEXT("有效图片: 正视图 + %d 个其他视角"), OtherViews.Num());

    AddSystemMessage(FString::Printf(
        TEXT("正在提交多视图图生3D任务，共%d张图片（正视图 + %d个其他视角）..."),
        TotalImages, OtherViews.Num()));

    TSharedPtr<FJsonObject> Params = MakeShareable(new FJsonObject());

    // 设置模型版本
    Params->SetStringField(TEXT("Model"), TEXT("3.1"));

    // 注意：不设置 Prompt！

    // 设置生成类型
    Params->SetStringField(TEXT("GenerateType"), TEXT("Normal"));

    // 是否开启 PBR
    Params->SetBoolField(TEXT("EnablePBR"), true);

    // 设置面数
    Params->SetNumberField(TEXT("FaceCount"), 500000.0);

    // 添加格式参数
    FString FormatStr = HunYuanAPI::GetFormatString(ConvertToAPIModelFormat(CurrentFormatPreference));
    if (!FormatStr.IsEmpty())
    {
        Params->SetStringField(TEXT("ResultFormat"), FormatStr);
    }

    // 1. 设置正视图作为主图（使用 ImageBase64）
    FString CleanFrontBase64 = FrontImage.Base64Data;
    int32 CommaIndex;
    if (CleanFrontBase64.FindChar(',', CommaIndex))
    {
        CleanFrontBase64 = CleanFrontBase64.RightChop(CommaIndex + 1);
    }
    Params->SetStringField(TEXT("ImageBase64"), CleanFrontBase64);
    UE_LOG(LogTemp, Log, TEXT("设置正视图作为主图，Base64长度=%d"), CleanFrontBase64.Len());

    // 2. 构建其他7个视角的多视图数组
    TArray<TSharedPtr<FJsonValue>> MultiViewArray;
    for (const auto& Image : OtherViews)
    {
        TSharedPtr<FJsonObject> ViewImageObj = MakeShareable(new FJsonObject());

        // 视角类型 - 使用 API 期望的值
        FString ViewTypeStr;
        switch (Image.ViewType)
        {
        case EViewType::Back:    ViewTypeStr = TEXT("back"); break;
        case EViewType::Left:    ViewTypeStr = TEXT("left"); break;
        case EViewType::Right:   ViewTypeStr = TEXT("right"); break;
        case EViewType::Top:     ViewTypeStr = TEXT("top"); break;
        case EViewType::Bottom:  ViewTypeStr = TEXT("bottom"); break;
        case EViewType::Left45:  ViewTypeStr = TEXT("left_front"); break;
        case EViewType::Right45: ViewTypeStr = TEXT("right_front"); break;
        default: continue;
        }
        ViewImageObj->SetStringField(TEXT("ViewType"), ViewTypeStr);

        // 清理 Base64 前缀
        FString CleanBase64 = Image.Base64Data;
        int32 CommaIndexView;
        if (CleanBase64.FindChar(',', CommaIndexView))
        {
            CleanBase64 = CleanBase64.RightChop(CommaIndexView + 1);
        }

        // 使用 ViewImageBase64 字段
        ViewImageObj->SetStringField(TEXT("ViewImageBase64"), CleanBase64);

        MultiViewArray.Add(MakeShareable(new FJsonValueObject(ViewImageObj)));

        UE_LOG(LogTemp, Log, TEXT("添加多视图图片: ViewType=%s, Base64长度=%d"),
            *ViewTypeStr, CleanBase64.Len());
    }

    if (MultiViewArray.Num() > 0)
    {
        Params->SetArrayField(TEXT("MultiViewImages"), MultiViewArray);
        UE_LOG(LogTemp, Log, TEXT("设置了 %d 个多视图图片"), MultiViewArray.Num());
    }

    // 打印完整的请求参数
    FString RequestJson;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestJson);
    FJsonSerializer::Serialize(Params.ToSharedRef(), Writer);
    UE_LOG(LogTemp, Warning, TEXT("========== 完整请求 JSON =========="));
    UE_LOG(LogTemp, Warning, TEXT("%s"), *RequestJson);
    UE_LOG(LogTemp, Warning, TEXT("===================================="));

    // 发送请求
    FHunYuanAPI::Get()->SendRequestAsync(
        TEXT("SubmitHunyuanTo3DProJob"),
        Params,
        FOnAPIRequestComplete::CreateLambda([this](const TSharedPtr<FJsonObject>& Result)
            {
                if (Result.IsValid())
                {
                    FString ResponseJson;
                    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseJson);
                    FJsonSerializer::Serialize(Result.ToSharedRef(), Writer);
                    UE_LOG(LogTemp, Log, TEXT("多视图响应: %s"), *ResponseJson);

                    const TSharedPtr<FJsonObject>* ErrorObj = nullptr;
                    if (Result->TryGetObjectField(TEXT("Error"), ErrorObj))
                    {
                        FString ErrorCode, ErrorMessage;
                        (*ErrorObj)->TryGetStringField(TEXT("Code"), ErrorCode);
                        (*ErrorObj)->TryGetStringField(TEXT("Message"), ErrorMessage);
                        OnMultiViewJobSubmitted(false, FString::Printf(TEXT("%s: %s"), *ErrorCode, *ErrorMessage));
                        return;
                    }

                    FString JobId;
                    if (Result->TryGetStringField(TEXT("JobId"), JobId))
                    {
                        OnMultiViewJobSubmitted(true, JobId);
                        return;
                    }

                    OnMultiViewJobSubmitted(false, TEXT("响应格式错误"));
                }
                else
                {
                    OnMultiViewJobSubmitted(false, TEXT("请求失败 - 无响应"));
                }
            }));
}

void SAIChatWindow::OnMultiViewJobSubmitted(bool bSuccess, const FString& JobIdOrError)
{
    if (bSuccess)
    {
        CurrentTask.JobId = JobIdOrError;
        CurrentTask.Status = Chat::EGenerationStatus::Waiting;

        AddSystemMessage(FString::Printf(
            TEXT("多视图任务提交成功！JobId: %s"),
            *JobIdOrError));

        UpdateProgress(CurrentTask);
        PollJobResult(JobIdOrError);
    }
    else
    {
        CurrentTask.Status = Chat::EGenerationStatus::Failed;
        AddErrorMessage(TEXT("多视图任务提交失败：") + JobIdOrError);
        ClearProgress();
    }
}

#undef LOCTEXT_NAMESPACE