// UI/SAIChatWindow.h
#pragma once

// === Unreal Engine 核心 ===
#include "CoreMinimal.h"

// === Slate基础组件 ===
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

// === Slate输入组件 ===
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SCheckBox.h"

// === Slate布局组件 ===
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"

// === Slate通知组件 ===
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

// === Slate图片组件 ===
#include "Widgets/Images/SImage.h"

// === Slate文本组件 ===
#include "Widgets/Text/STextBlock.h"

// === 项目基础类型 ===
#include "ChatTypes.h"

// === 项目UI组件 ===
#include "ModelInfoWidget.h"

// === 知识图谱搜索组件 ===
#include "UI/SKnowledgeGraphSearchWidget.h"

// === 知识图谱类型定义===
#include "RagTypes.h"          // 来自 UE_RAG_SDK 插件
#include "UERagSubsystem.h" 

// === 下载相关 ===
#include "Download/ModelDownloader.h"
#include "Download/ZipExtractor.h"

// === 导入相关 ===
#include "Import/ModelImportManager.h"

// === API和配置 ===
#include "API/HunYuanAPI.h"
#include "Config/HunYuanConfigManager.h"

using namespace HunYuanAPI;

struct FAsyncImageLoadResult
{
    EViewType ViewType = EViewType::Front;
    TSharedPtr<TArray<uint8>> RawData;
    TSharedPtr<TArray<uint8>> ThumbnailData;  // 缩略图数据
    int32 Width = 0;
    int32 Height = 0;
    int32 ThumbnailWidth = 0;
    int32 ThumbnailHeight = 0;
    FString Base64Data;
    FString FilePath;
    FString FileName;
    int64 FileSize = 0;
    bool bSuccess = false;

    // 创建纹理（可选择创建缩略图或原图）
    UTexture2D* CreateTexture(bool bUseThumbnail = false) const;

    // 获取缩略图纹理（快速显示）
    UTexture2D* CreateThumbnailTexture() const
    {
        return CreateTexture(true);
    }
};

// 模型选项
struct FModelOption
{
    FString DisplayName;
    FString ModelName;

    FModelOption(const FString& InDisplayName, const FString& InModelName)
        : DisplayName(InDisplayName), ModelName(InModelName) {
    }
};

// 格式偏好枚举
UENUM()
enum class EModelFormatPreference : uint8
{
    Default = 0 UMETA(DisplayName = "默认 (OBJ+GLB)"),
    GLB = 1 UMETA(DisplayName = "GLB格式"),
    OBJ = 2 UMETA(DisplayName = "OBJ格式 (ZIP)"),
    STL = 3 UMETA(DisplayName = "STL格式 (3D打印)"),
    USDZ = 4 UMETA(DisplayName = "USDZ格式 (AR)"),
    FBX = 5 UMETA(DisplayName = "FBX格式 (动画)")
};

// 视图图片项
struct FViewImageItem
{
    TSharedPtr<SImage> ImageWidget;
    TSharedPtr<FSlateBrush> Brush;
    FString FilePath;
    FString Base64Data;
    bool bIsLoaded = false;
    bool bHasFullTexture = false;  // 标记是否已加载原图
    EViewType ViewType;
};

class SAIChatWindow : public SCompoundWidget
{
public:

    SLATE_BEGIN_ARGS(SAIChatWindow) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    ~SAIChatWindow();

    // ========== 多视图图片上传 ==========
    TSharedRef<SWidget> BuildMultiViewUploadSection();
    FReply OnUploadViewImageClicked(EViewType ViewType);
    FReply OnClearViewImageClicked(EViewType ViewType);
    void HandleViewImageUploaded(EViewType ViewType, const FString& ImagePath);
    bool LoadViewImagePreview(EViewType ViewType, const FString& ImagePath);
    FText GetViewUploadButtonText(EViewType ViewType) const;
    bool IsViewImageValid(EViewType ViewType) const;
    int32 GetValidViewCount() const;
    TSharedRef<SWidget> BuildImageInputSection();
    void OnSingleImageModeChanged(ECheckBoxState NewState);
    void OnMultiViewModeChanged(ECheckBoxState NewState);
    TSharedRef<SWidget> CreateViewSlots(const TArray<EViewType>& ViewOrder, const TMap<EViewType, FString>& ViewNames);

    // ========== 多视图提交 ==========
    void SubmitMultiViewTo3D(const FMultiViewInput& MultiViewInput);
    void OnMultiViewJobSubmitted(bool bSuccess, const FString& JobIdOrError);

    // ========== 异步加载结果回调 ==========
    void OnAsyncImageLoaded(EViewType ViewType, const FAsyncImageLoadResult& Result);
    void SetViewImageLoadingState(EViewType ViewType, bool bIsLoading);
    void UpdateViewImagePreview(EViewType ViewType, UTexture2D* Texture,
        const FString& Base64Data, const FString& FileName, int64 FileSize);

private:
    // ========== UI 构建 ==========
    TSharedRef<SWidget> BuildApiConfigSection();
    TSharedRef<SWidget> BuildImportOptionsSection();
    TSharedRef<SWidget> BuildModelSelectionSection();
    TSharedRef<SWidget> BuildFormatSelectionSection();
    TSharedRef<SWidget> BuildImageUploadSection();
    TSharedRef<SWidget> BuildChatHistorySection();
    TSharedRef<SWidget> BuildProgressSection();
    TSharedRef<SWidget> BuildInputSection();
    TSharedRef<SWidget> BuildMainPanel();
    TSharedRef<SWidget> BuildSidebar();

    // ========== 消息处理 ==========
    void AddMessage(const Chat::FChatMessage& Message);
    void AddSystemMessage(const FString& Content);
    void AddErrorMessage(const FString& Content);
    void AddUserMessage(const FString& Content);
    TSharedRef<ITableRow> GenerateMessageRow(TSharedPtr<Chat::FChatMessage> Item, const TSharedRef<STableViewBase>& OwnerTable);

    // ========== 图片上传 ==========
    FReply OnUploadImageButtonClicked();
    FReply OnClearImageButtonClicked();
    void HandleUploadedImage(const FString& ImagePath);
    bool LoadImagePreview(const FString& ImagePath);

    // ========== 配置管理 ==========
    void LoadConfig();
    void SaveConfig();
    void OnConfigChanged(const HunYuanConfig::FConfigData& NewConfig);
    void UpdateCredentialsUI();
    FReply OnSaveConfigButtonClicked();
    void OnRememberPasswordCheckStateChanged(ECheckBoxState NewState);

    // ========== 模型选择 ==========
    TSharedRef<SWidget> GenerateModelOptionWidget(TSharedPtr<FModelOption> InOption);
    void OnModelSelectionChanged(TSharedPtr<FModelOption> NewSelection, ESelectInfo::Type SelectInfo);
    FText GetCurrentModelText() const;

    // ========== 生成任务 ==========
    FReply OnSendButtonClicked();
    void OnInputTextCommitted(const FText& NewText, ETextCommit::Type CommitType);
    void SubmitTextTo3D(const FString& Prompt);
    void SubmitImageTo3D(const FString& ImagePath);
    void OnJobSubmitted(bool bSuccess, const FString& JobIdOrError);
    void PollJobResult(const FString& JobId);
    void OnJobQueried(bool bSuccess, const TSharedPtr<FJsonObject>& Result);
    void SubmitGenerationTask();

    // ========== 格式选择 ==========
    TSharedPtr<SComboBox<TSharedPtr<EModelFormatPreference>>> FormatComboBox;
    TArray<TSharedPtr<EModelFormatPreference>> FormatOptions;
    EModelFormatPreference CurrentFormatPreference;

    TSharedRef<SWidget> GenerateFormatOptionWidget(TSharedPtr<EModelFormatPreference> InOption);
    void OnFormatSelectionChanged(TSharedPtr<EModelFormatPreference> NewSelection, ESelectInfo::Type SelectInfo);
    FText GetCurrentFormatText() const;
    FText GetFormatDescription(EModelFormatPreference Format) const;
    EModelFormat ConvertToAPIModelFormat(EModelFormatPreference Preference) const;
    void SaveFormatPreference();
    void LoadFormatPreference();
    EModelFormatPreference GetCurrentFormatPreference() const { return CurrentFormatPreference; }

    // ========== 下载处理 ==========
    FReply OnSelectDownloadDirectoryClicked();
    void OnDownloadProgress(const Download::FDownloadItem& Item);
    void OnDownloadItemComplete(const Download::FDownloadItem& Item);
    void HandleDownloadedFile(const FString& FilePath, const FString& JobId);
    void FinishDownloadProcessing(const FModelInfo& ModelInfo, bool bExtractSuccess);

    // ========== 模型处理 ==========
    void ShowModelPreview(const FModelInfo& ModelInfo);
    void PreviewModel(const FModelInfo& ModelInfo);
    void DeleteModelFile(const FModelInfo& ModelInfo);
    void OpenModelFolder(const FModelInfo& ModelInfo);
    void ImportModel(const FModelInfo& ModelInfo);
    void ShowImportErrorNotification(const FString& ErrorMessage);
    void HandleModelImported(bool bSuccess, const FString& AssetPath);
    void RefreshModelList();

    // ========== 历史管理 ==========
    void LoadDownloadHistory();
    void SaveDownloadHistory();
    void AddToHistory(const FModelInfo& ModelInfo);

    // ========== UI状态更新 ==========
    void UpdateProgress(const Chat::FGenerationTask& Task);
    void ClearProgress();
    EVisibility GetProgressVisibility() const;
    TOptional<float> GetProgressPercent() const;

    // ========== 知识图谱搜索相关 ==========
    void OnSearchResultSelected(const FSourceDocument& Document);
    void ShowReferencePanel(const FSourceDocument& Document);
    void HideReferencePanel();
    FReply OnToggleSidebar();
    void OnSidebarResized(float NewSize);
    FReply OnCopyReferenceToPrompt();

    // ========== 辅助函数 ==========
    void AddKnowledgeGraphSearchPanel();
    void OnConfigChanged();

    // ========== 核心组件 ==========
    TSharedPtr<FModelDownloader> Downloader;
    TSharedPtr<FModelImportManager> ImportManager;

    // ========== 状态 ==========
    Chat::FGenerationTask CurrentTask;
    TArray<TSharedPtr<Chat::FChatMessage>> Messages;
    TArray<FModelInfo> DownloadHistory;

    // ========== 图片上传 ==========
    FString CurrentImagePath;
    TSharedPtr<FSlateBrush> ImageBrush;

    // ========== 模型选项 ==========
    TArray<TSharedPtr<FModelOption>> ModelOptions;
    TSharedPtr<FModelOption> CurrentSelectedModel;
    FModelInfo CurrentModelInfo;

    // ========== UI控件 ==========
    TSharedPtr<SEditableTextBox> InputTextBox;
    TSharedPtr<SListView<TSharedPtr<Chat::FChatMessage>>> MessagesListView;
    TSharedPtr<SImage> ImagePreviewWidget;
    TSharedPtr<STextBlock> ImageInfoText;
    TSharedPtr<SProgressBar> UploadProgressBar;
    TSharedPtr<SBorder> ProgressContainer;
    TSharedPtr<STextBlock> ProgressText;
    TSharedPtr<SProgressBar> ProgressBar;
    TMap<EViewType, FViewImageItem> ViewImages;
    TMap<EViewType, TSharedPtr<SButton>> ViewClearButtons;
    TMap<EViewType, TSharedPtr<STextBlock>> ViewInfoTexts;
    TSharedPtr<SScrollBox> MultiViewScrollBox;    // 多视图滚动区域

    // 多视图模式开关
    TSharedPtr<SCheckBox> MultiViewModeCheckBox;
    bool bUseMultiViewMode = false;

    // API配置控件
    TSharedPtr<SEditableTextBox> SecretIdInput;
    TSharedPtr<SEditableTextBox> SecretKeyInput;
    TSharedPtr<SCheckBox> RememberPasswordCheckBox;

    // 导入选项控件
    TSharedPtr<SCheckBox> AutoImportCheckBox;
    TSharedPtr<SCheckBox> ShowPreviewCheckBox;
    TSharedPtr<STextBlock> DownloadDirText;

    // ========== 知识图谱搜索相关成员 ==========
    TSharedPtr<SKnowledgeGraphSearchWidget> KnowledgeGraphSearch;
    TSharedPtr<SBox> ReferencePanel;
    TSharedPtr<STextBlock> ReferenceTitleText;
    TSharedPtr<STextBlock> ReferenceContentText;

    // 侧边栏状态
    bool bSidebarVisible = true;
    float CachedSidebarSize = 0.35f;

    // ========== 窗口 ==========
    TSharedPtr<SWindow> ModelInfoWindow;
};