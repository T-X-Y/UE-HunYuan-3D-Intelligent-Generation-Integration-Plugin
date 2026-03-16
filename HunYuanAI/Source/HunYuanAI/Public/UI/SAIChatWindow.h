#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "ChatTypes.h"
#include "ModelInfoWidget.h"
#include "Download/ModelDownloader.h"
#include "Download/ZipExtractor.h"
#include "Import/ModelImportManager.h"
#include "API/HunYuanAPI.h"
#include "Config/HunYuanConfigManager.h"

// 模型选项
struct FModelOption
{
    FString DisplayName;
    FString ModelName;

    FModelOption(const FString& InDisplayName, const FString& InModelName)
        : DisplayName(InDisplayName), ModelName(InModelName) {
    }
};

class SAIChatWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SAIChatWindow) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    ~SAIChatWindow();

private:
    // UI 构建
    TSharedRef<SWidget> BuildApiConfigSection();
    TSharedRef<SWidget> BuildImportOptionsSection();
    TSharedRef<SWidget> BuildModelSelectionSection();
    TSharedRef<SWidget> BuildImageUploadSection();
    TSharedRef<SWidget> BuildChatHistorySection();
    TSharedRef<SWidget> BuildProgressSection();
    TSharedRef<SWidget> BuildInputSection();

    // 消息处理
    void AddMessage(const Chat::FChatMessage& Message);
    void AddSystemMessage(const FString& Content);
    void AddErrorMessage(const FString& Content);
    void AddUserMessage(const FString& Content);

    TSharedRef<ITableRow> GenerateMessageRow(TSharedPtr<Chat::FChatMessage> Item, const TSharedRef<STableViewBase>& OwnerTable);

    // 图片上传
    FReply OnUploadImageButtonClicked();
    FReply OnClearImageButtonClicked();
    void HandleUploadedImage(const FString& ImagePath);
    bool LoadImagePreview(const FString& ImagePath);

    // 配置管理
    void LoadConfig();
    void SaveConfig();
    void OnConfigChanged(const HunYuanConfig::FConfigData& NewConfig);
    void UpdateCredentialsUI();

    FReply OnSaveConfigButtonClicked();
    void OnRememberPasswordCheckStateChanged(ECheckBoxState NewState);

    // 模型选择
    TSharedRef<SWidget> GenerateModelOptionWidget(TSharedPtr<FModelOption> InOption);
    void OnModelSelectionChanged(TSharedPtr<FModelOption> NewSelection, ESelectInfo::Type SelectInfo);
    FText GetCurrentModelText() const;

    // 生成任务
    FReply OnSendButtonClicked();
    void OnInputTextCommitted(const FText& NewText, ETextCommit::Type CommitType);

    void SubmitTextTo3D(const FString& Prompt);
    void SubmitImageTo3D(const FString& ImagePath);
    void OnJobSubmitted(bool bSuccess, const FString& JobIdOrError);
    void PollJobResult(const FString& JobId);
    void OnJobQueried(bool bSuccess, const TSharedPtr<FJsonObject>& Result);

    // 下载处理
    FReply OnSelectDownloadDirectoryClicked();
    void OnDownloadProgress(const Download::FDownloadItem& Item);
    void OnDownloadComplete(bool bSuccess, const FString& FilePath);
    void OnDownloadItemComplete(const Download::FDownloadItem& Item);
    void HandleDownloadedFile(const FString& FilePath, const FString& JobId);

    // 模型处理
    void ShowModelPreview(const FModelInfo& ModelInfo);
    void PreviewModel(const FModelInfo& ModelInfo);
    void ImportModelFromFolder(const FModelInfo& ModelInfo);
    void DeleteModelFile(const FModelInfo& ModelInfo);
    void OpenModelFolder(const FModelInfo& ModelInfo);

    // 导入结果处理
    void HandleModelImported(bool bSuccess, const FString& AssetPath);

    // 刷新模型列表
    void RefreshModelList();

    // 历史管理
    void LoadDownloadHistory();
    void SaveDownloadHistory();
    void AddToHistory(const FModelInfo& ModelInfo);

    // UI 状态更新
    void UpdateProgress(const Chat::FGenerationTask& Task);
    void ClearProgress();
    EVisibility GetProgressVisibility() const;
    TOptional<float> GetProgressPercent() const;

private:
    // 核心组件
    TSharedPtr<FModelDownloader> Downloader;
    TSharedPtr<FModelImportManager> ImportManager;

    // 状态
    Chat::FGenerationTask CurrentTask;
    TArray<TSharedPtr<Chat::FChatMessage>> Messages;
    TArray<FModelInfo> DownloadHistory;

    // 图片上传
    FString CurrentImagePath;
    TSharedPtr<FSlateBrush> ImageBrush;

    // 模型选项
    TArray<TSharedPtr<FModelOption>> ModelOptions;
    TSharedPtr<FModelOption> CurrentSelectedModel;
    FModelInfo CurrentModelInfo;  // 如果需要保存当前选中的模型

    // UI控件
    TSharedPtr<SEditableTextBox> InputTextBox;
    TSharedPtr<SListView<TSharedPtr<Chat::FChatMessage>>> MessagesListView;
    TSharedPtr<SImage> ImagePreviewWidget;
    TSharedPtr<STextBlock> ImageInfoText;
    TSharedPtr<SProgressBar> UploadProgressBar;
    TSharedPtr<SBorder> ProgressContainer;
    TSharedPtr<STextBlock> ProgressText;
    TSharedPtr<SProgressBar> ProgressBar;

    // API配置控件
    TSharedPtr<SEditableTextBox> SecretIdInput;
    TSharedPtr<SEditableTextBox> SecretKeyInput;
    TSharedPtr<SCheckBox> RememberPasswordCheckBox;

    // 导入选项控件
    TSharedPtr<SCheckBox> AutoImportCheckBox;
    TSharedPtr<SCheckBox> ShowPreviewCheckBox;
    TSharedPtr<STextBlock> DownloadDirText;

    // 窗口
    TSharedPtr<SWindow> ModelInfoWindow;
};