// SAIChatWindowUpload.cpp
// 图片上传：单图模式、多视图模式切换、图片上传相关

// === 核心头文件 ===
#include "CoreMinimal.h"
#include "UI/SAIChatWindow.h"

// === Slate UI组件 ===
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"

// === 图片处理 ===
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Engine/Texture2D.h"

// === 桌面平台功能 ===
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"

// === 文件系统 ===
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"

#define LOCTEXT_NAMESPACE "SAIChatWindow"

// ==================== 单图上传 ====================

FReply SAIChatWindow::OnUploadImageButtonClicked()
{
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (DesktopPlatform)
    {
        TArray<FString> OutFiles;
        if (DesktopPlatform->OpenFileDialog(
            FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
            TEXT("选择图片"),
            FPaths::ProjectContentDir(),
            TEXT(""),
            TEXT("图片文件|*.jpg;*.jpeg;*.png;*.bmp|所有文件|*.*"),
            EFileDialogFlags::None,
            OutFiles) && OutFiles.Num() > 0)
        {
            HandleUploadedImage(OutFiles[0]);
        }
    }
    return FReply::Handled();
}

FReply SAIChatWindow::OnClearImageButtonClicked()
{
    CurrentImagePath.Empty();
    ImageInfoText->SetText(LOCTEXT("NoImage", "未选择图片"));
    ImageBrush->SetResourceObject(nullptr);
    ImagePreviewWidget->SetImage(nullptr);
    return FReply::Handled();
}

void SAIChatWindow::HandleUploadedImage(const FString& ImagePath)
{
    CurrentImagePath = ImagePath;

    FString FileName = FPaths::GetCleanFilename(ImagePath);
    int64 FileSize = IFileManager::Get().FileSize(*ImagePath);

    FString SizeString = FString::Printf(TEXT("%.1f KB"), FileSize / 1024.0f);
    ImageInfoText->SetText(FText::Format(
        LOCTEXT("ImageSelected", "已选择: {0} ({1})"),
        FText::FromString(FileName),
        FText::FromString(SizeString)));

    LoadImagePreview(ImagePath);
}

bool SAIChatWindow::LoadImagePreview(const FString& ImagePath)
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

    // 更新 Slate 画笔
    ImageBrush->SetResourceObject(Texture);
    ImageBrush->ImageSize = FVector2D(ImageWrapper->GetWidth(), ImageWrapper->GetHeight());
    ImagePreviewWidget->SetImage(ImageBrush.Get());

    return true;
}

// ==================== 图片输入区域（模式切换） ====================

TSharedRef<SWidget> SAIChatWindow::BuildImageInputSection()
{
    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
        .BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f))
        .Padding(10)
        [
            SNew(SVerticalBox)

                // 标题和模式切换
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
                                .Text(LOCTEXT("ImageInput", "图片输入"))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                        ]

                        // 模式切换按钮
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .HAlign(HAlign_Right)
                        .VAlign(VAlign_Center)
                        [
                            SNew(SHorizontalBox)

                                // 单图模式
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(0, 0, 10, 0)
                                [
                                    SNew(SCheckBox)
                                        .Style(FCoreStyle::Get(), "RadioButton")
                                        .IsChecked_Lambda([this]()
                                            {
                                                return !bUseMultiViewMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
                                            })
                                        .OnCheckStateChanged(this, &SAIChatWindow::OnSingleImageModeChanged)
                                        [
                                            SNew(STextBlock)
                                                .Text(LOCTEXT("SingleImageMode", "单图模式"))
                                        ]
                                ]

                            // 多视图模式
                            + SHorizontalBox::Slot()
                                .AutoWidth()
                                [
                                    SNew(SCheckBox)
                                        .Style(FCoreStyle::Get(), "RadioButton")
                                        .IsChecked_Lambda([this]()
                                            {
                                                return bUseMultiViewMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
                                            })
                                        .OnCheckStateChanged(this, &SAIChatWindow::OnMultiViewModeChanged)
                                        [
                                            SNew(STextBlock)
                                                .Text(LOCTEXT("MultiViewMode", "多视图模式"))
                                        ]
                                ]
                        ]
                ]

            // 单图上传区域（单图模式可见）
            + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SBox)
                        .Visibility_Lambda([this]()
                            {
                                return !bUseMultiViewMode ? EVisibility::Visible : EVisibility::Collapsed;
                            })
                        [
                            BuildImageUploadSection()
                        ]
                ]

            // 多视图上传区域（多视图模式可见）
            + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SBox)
                        .Visibility_Lambda([this]()
                            {
                                return bUseMultiViewMode ? EVisibility::Visible : EVisibility::Collapsed;
                            })
                        [
                            BuildMultiViewUploadSection()
                        ]
                ]
        ];
}

void SAIChatWindow::OnSingleImageModeChanged(ECheckBoxState NewState)
{
    if (NewState == ECheckBoxState::Checked)
    {
        bUseMultiViewMode = false;
    }
}

void SAIChatWindow::OnMultiViewModeChanged(ECheckBoxState NewState)
{
    if (NewState == ECheckBoxState::Checked)
    {
        bUseMultiViewMode = true;

        // 清理单图数据
        CurrentImagePath.Empty();
        ImageBrush->SetResourceObject(nullptr);

        if (ImagePreviewWidget.IsValid())
        {
            ImagePreviewWidget->SetImage(nullptr);
        }

        if (ImageInfoText.IsValid())
        {
            ImageInfoText->SetText(LOCTEXT("NoImage", "未选择图片"));
        }
    }
}

#undef LOCTEXT_NAMESPACE