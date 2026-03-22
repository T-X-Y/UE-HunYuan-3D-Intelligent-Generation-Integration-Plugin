// SAIChatWindow.cpp
// 核心逻辑：构造/析构、消息处理、配置管理、模型选择、格式选择、生成任务、下载处理、模型处理、历史管理、UI状态更新、知识图谱集成

#include "UI/SAIChatWindow.h"
#include "UI/SKnowledgeGraphSearchWidget.h"
#include "HunYuanAI.h"
#include "API/HunYuanAPITypes.h"
#include "Logging/HunYuanLogging.h"
#include "Download/DownloadHandlerFactory.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"

#define LOCTEXT_NAMESPACE "SAIChatWindow"

// ==================== 构造/析构 ====================

void SAIChatWindow::Construct(const FArguments& InArgs)
{
    // 初始化组件
    Downloader = MakeShareable(new FModelDownloader());
    ImportManager = FModelImportManager::Get();

    // 初始化模型选项
    ModelOptions.Add(MakeShareable(new FModelOption(TEXT("混元生3D 3.0"), TEXT("3.0"))));
    ModelOptions.Add(MakeShareable(new FModelOption(TEXT("混元生3D 3.1"), TEXT("3.1"))));
    CurrentSelectedModel = ModelOptions[0];

    // 初始化图片画笔
    ImageBrush = MakeShareable(new FSlateBrush());
    ImageBrush->DrawAs = ESlateBrushDrawType::Image;

    // 添加格式选项
    FormatOptions.Add(MakeShareable(new EModelFormatPreference(EModelFormatPreference::Default)));
    FormatOptions.Add(MakeShareable(new EModelFormatPreference(EModelFormatPreference::GLB)));
    FormatOptions.Add(MakeShareable(new EModelFormatPreference(EModelFormatPreference::OBJ)));
    FormatOptions.Add(MakeShareable(new EModelFormatPreference(EModelFormatPreference::STL)));
    FormatOptions.Add(MakeShareable(new EModelFormatPreference(EModelFormatPreference::USDZ)));
    FormatOptions.Add(MakeShareable(new EModelFormatPreference(EModelFormatPreference::FBX)));

    // 加载保存的格式偏好
    LoadFormatPreference();

    // 添加知识图谱搜索面板
    AddKnowledgeGraphSearchPanel();

    // 左右分屏布局
    ChildSlot
        [
            SNew(SSplitter)
                .Orientation(Orient_Horizontal)
                .ResizeMode(ESplitterResizeMode::FixedPosition)
                + SSplitter::Slot()
                .Value(0.65f)
                [
                    BuildMainPanel()
                ]
                + SSplitter::Slot()
                .Value(0.35f)
                .OnSlotResized(SSplitter::FOnSlotResized::CreateSP(this, &SAIChatWindow::OnSidebarResized))
                [
                    BuildSidebar()
                ]
        ];

    // 加载配置
    LoadConfig();

    // 绑定事件
    Downloader->OnDownloadProgress.AddSP(this, &SAIChatWindow::OnDownloadProgress);

    // 加载历史
    LoadDownloadHistory();

    // 添加欢迎消息
    Chat::FChatMessage WelcomeMsg;
    WelcomeMsg.Type = Chat::EMessageType::System;
    WelcomeMsg.Sender = TEXT("系统");
    WelcomeMsg.Content = TEXT("欢迎使用混元3D生成工具！请输入文字描述或上传图片开始生成3D模型。\n右侧知识图谱搜索可帮助您查找相关文档和资产参考。");
    AddMessage(WelcomeMsg);
}

SAIChatWindow::~SAIChatWindow()
{
    if (Downloader.IsValid())
    {
        Downloader->CancelAllDownloads();
    }

    SaveDownloadHistory();

    if (ModelInfoWindow.IsValid())
    {
        ModelInfoWindow->RequestDestroyWindow();
    }
}

// ==================== 消息处理 ====================

void SAIChatWindow::AddMessage(const Chat::FChatMessage& Message)
{
    AsyncTask(ENamedThreads::GameThread, [this, Message]()
        {
            Messages.Add(MakeShareable(new Chat::FChatMessage(Message)));

            if (MessagesListView.IsValid())
            {
                MessagesListView->RequestListRefresh();
                MessagesListView->ScrollToBottom();
            }
        });
}

void SAIChatWindow::AddSystemMessage(const FString& Content)
{
    Chat::FChatMessage Message;
    Message.Type = Chat::EMessageType::System;
    Message.Sender = TEXT("系统");
    Message.Content = Content;
    AddMessage(Message);
}

void SAIChatWindow::AddErrorMessage(const FString& Content)
{
    Chat::FChatMessage Message;
    Message.Type = Chat::EMessageType::Error;
    Message.Sender = TEXT("错误");
    Message.Content = Content;
    AddMessage(Message);
}

void SAIChatWindow::AddUserMessage(const FString& Content)
{
    Chat::FChatMessage Message;
    Message.Type = Chat::EMessageType::User;
    Message.Sender = TEXT("用户");
    Message.Content = Content;
    AddMessage(Message);
}

TSharedRef<ITableRow> SAIChatWindow::GenerateMessageRow(
    TSharedPtr<Chat::FChatMessage> Item,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    FLinearColor TextColor = FLinearColor::White;

    switch (Item->Type)
    {
    case Chat::EMessageType::User:
        TextColor = FLinearColor(0.2f, 0.8f, 1.0f);
        break;
    case Chat::EMessageType::System:
        TextColor = FLinearColor(0.4f, 1.0f, 0.4f);
        break;
    case Chat::EMessageType::Error:
        TextColor = FLinearColor(1.0f, 0.4f, 0.4f);
        break;
    case Chat::EMessageType::Progress:
        TextColor = FLinearColor(1.0f, 1.0f, 0.4f);
        break;
    default:
        break;
    }

    return SNew(STableRow<TSharedPtr<Chat::FChatMessage>>, OwnerTable)
        [
            SNew(STextBlock)
                .Text(Item->GetDisplayText())
                .AutoWrapText(true)
                .ColorAndOpacity(TextColor)
        ];
}

// ==================== 配置管理 ====================

void SAIChatWindow::LoadConfig()
{
    auto ConfigManager = FHunYuanConfigManager::Get();
    auto Config = ConfigManager->GetConfig();

    // 更新UI
    SecretIdInput->SetText(FText::FromString(Config.SecretId));
    SecretKeyInput->SetText(FText::FromString(Config.SecretKey));
    RememberPasswordCheckBox->SetIsChecked(Config.bRememberPassword ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
    AutoImportCheckBox->SetIsChecked(Config.bAutoImport ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
    ShowPreviewCheckBox->SetIsChecked(Config.bShowPreviewAfterDownload ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
    DownloadDirText->SetText(FText::FromString(Config.DownloadDirectory));
    CurrentFormatPreference = static_cast<EModelFormatPreference>(Config.LastUsedFormat);

    // 绑定配置变更事件
    ConfigManager->OnConfigChanged.AddSP(this, &SAIChatWindow::OnConfigChanged);

    // 设置API凭证
    if (Config.HasValidCredentials())
    {
        FHunYuanAPI::Get()->SetCredentials(Config.SecretId, Config.SecretKey, Config.Region);
    }
}

void SAIChatWindow::OnConfigChanged(const HunYuanConfig::FConfigData& NewConfig)
{
    // 更新UI
    SecretIdInput->SetText(FText::FromString(NewConfig.SecretId));
    SecretKeyInput->SetText(FText::FromString(NewConfig.SecretKey));
    RememberPasswordCheckBox->SetIsChecked(NewConfig.bRememberPassword ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
    DownloadDirText->SetText(FText::FromString(NewConfig.DownloadDirectory));

    // 更新API凭证
    if (NewConfig.HasValidCredentials())
    {
        FHunYuanAPI::Get()->SetCredentials(NewConfig.SecretId, NewConfig.SecretKey, NewConfig.Region);
    }
}

FReply SAIChatWindow::OnSaveConfigButtonClicked()
{
    auto ConfigManager = FHunYuanConfigManager::Get();
    auto Config = ConfigManager->GetConfig();

    Config.SecretId = SecretIdInput->GetText().ToString();
    Config.SecretKey = SecretKeyInput->GetText().ToString();
    Config.bRememberPassword = RememberPasswordCheckBox->IsChecked();
    Config.bAutoImport = AutoImportCheckBox->IsChecked();
    Config.bShowPreviewAfterDownload = ShowPreviewCheckBox->IsChecked();

    ConfigManager->UpdateConfig(Config);

    // 保存配置后立即设置API凭证
    if (Config.HasValidCredentials())
    {
        FHunYuanAPI::Get()->SetCredentials(Config.SecretId, Config.SecretKey, Config.Region);
        AddSystemMessage(TEXT("凭证已生效"));
    }
    else
    {
        AddSystemMessage(TEXT("配置已保存，但凭证不完整"));
    }

    return FReply::Handled();
}

void SAIChatWindow::OnRememberPasswordCheckStateChanged(ECheckBoxState NewState)
{
    auto ConfigManager = FHunYuanConfigManager::Get();
    auto Config = ConfigManager->GetConfig();

    Config.bRememberPassword = (NewState == ECheckBoxState::Checked);

    if (!Config.bRememberPassword)
    {
        Config.SecretId.Empty();
        Config.SecretKey.Empty();
        SecretIdInput->SetText(FText::GetEmpty());
        SecretKeyInput->SetText(FText::GetEmpty());
    }

    ConfigManager->UpdateConfig(Config);
    AddSystemMessage(Config.bRememberPassword ? TEXT("已启用记住密码") : TEXT("已禁用记住密码"));
}

void SAIChatWindow::UpdateCredentialsUI()
{
    auto Config = FHunYuanConfigManager::Get()->GetConfig();

    if (!Config.bRememberPassword && Config.HasValidCredentials())
    {
        // 不记住密码但有保存的凭证，显示提示文本
        SecretIdInput->SetHintText(FText::Format(
            LOCTEXT("SecretIdHintWithSaved", "SecretId已保存 (勾选“记住密码”后显示) - {0}..."),
            FText::FromString(Config.SecretId.Left(8))));

        SecretKeyInput->SetHintText(LOCTEXT("SecretKeyHintWithSaved", "SecretKey已保存 (勾选“记住密码”后显示)"));

        SecretIdInput->SetText(FText::GetEmpty());
        SecretKeyInput->SetText(FText::GetEmpty());
    }
    else
    {
        // 正常模式
        SecretIdInput->SetHintText(LOCTEXT("SecretIdHint", "输入SecretId"));
        SecretKeyInput->SetHintText(LOCTEXT("SecretKeyHint", "输入SecretKey"));
    }
}

// ==================== 模型选择 ====================

TSharedRef<SWidget> SAIChatWindow::GenerateModelOptionWidget(TSharedPtr<FModelOption> InOption)
{
    return SNew(STextBlock)
        .Text(FText::FromString(InOption->DisplayName));
}

void SAIChatWindow::OnModelSelectionChanged(TSharedPtr<FModelOption> NewSelection, ESelectInfo::Type SelectInfo)
{
    if (NewSelection.IsValid())
    {
        CurrentSelectedModel = NewSelection;
    }
}

FText SAIChatWindow::GetCurrentModelText() const
{
    return CurrentSelectedModel.IsValid()
        ? FText::FromString(CurrentSelectedModel->DisplayName)
        : LOCTEXT("NoModel", "选择模型");
}

// ==================== 格式选择 ====================

TSharedRef<SWidget> SAIChatWindow::BuildFormatSelectionSection()
{
    TSharedPtr<EModelFormatPreference> InitiallySelected = nullptr;
    for (auto& Option : FormatOptions)
    {
        if (*Option == CurrentFormatPreference)
        {
            InitiallySelected = Option;
            break;
        }
    }

    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
        .BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f))
        .Padding(10)
        [
            SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 10)
                [
                    SNew(STextBlock)
                        .Text(LOCTEXT("FormatSelection", "输出格式选择"))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0, 0, 10, 0)
                        [
                            SNew(STextBlock)
                                .Text(LOCTEXT("SelectFormat", "选择格式:"))
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        [
                            SAssignNew(FormatComboBox, SComboBox<TSharedPtr<EModelFormatPreference>>)
                                .OptionsSource(&FormatOptions)
                                .InitiallySelectedItem(InitiallySelected)
                                .OnGenerateWidget(this, &SAIChatWindow::GenerateFormatOptionWidget)
                                .OnSelectionChanged(this, &SAIChatWindow::OnFormatSelectionChanged)
                                .Content()
                                [
                                    SNew(STextBlock)
                                        .Text(this, &SAIChatWindow::GetCurrentFormatText)
                                ]
                        ]

                    + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .HAlign(HAlign_Right)
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Text_Lambda([this]()
                                    {
                                        return GetFormatDescription(CurrentFormatPreference);
                                    })
                                .ColorAndOpacity(FLinearColor::Gray)
                                .Font(FCoreStyle::GetDefaultFontStyle("Italic", 10))
                        ]
                ]

            + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 5, 0, 0)
                [
                    SNew(STextBlock)
                        .Text(LOCTEXT("FormatHint", "注：如果选择的格式不可用，将自动降级为默认格式"))
                        .ColorAndOpacity(FLinearColor::Yellow)
                        .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                ]
        ];
}

TSharedRef<SWidget> SAIChatWindow::GenerateFormatOptionWidget(TSharedPtr<EModelFormatPreference> InOption)
{
    if (!InOption.IsValid())
    {
        return SNew(STextBlock).Text(LOCTEXT("Invalid", "无效"));
    }

    FText DisplayText;
    switch (*InOption)
    {
    case EModelFormatPreference::Default:
        DisplayText = LOCTEXT("FormatDefault", "默认格式 (OBJ + GLB)");
        break;
    case EModelFormatPreference::GLB:
        DisplayText = LOCTEXT("FormatGLB", "GLB格式 (二进制glTF)");
        break;
    case EModelFormatPreference::OBJ:
        DisplayText = LOCTEXT("FormatOBJ", "OBJ格式 (包含MTL纹理)");
        break;
    case EModelFormatPreference::STL:
        DisplayText = LOCTEXT("FormatSTL", "STL格式 (3D打印)");
        break;
    case EModelFormatPreference::USDZ:
        DisplayText = LOCTEXT("FormatUSDZ", "USDZ格式 (AR增强现实)");
        break;
    case EModelFormatPreference::FBX:
        DisplayText = LOCTEXT("FormatFBX", "FBX格式 (动画/游戏)");
        break;
    default:
        DisplayText = LOCTEXT("FormatUnknown", "未知格式");
        break;
    }

    return SNew(STextBlock).Text(DisplayText);
}

void SAIChatWindow::OnFormatSelectionChanged(TSharedPtr<EModelFormatPreference> NewSelection, ESelectInfo::Type SelectInfo)
{
    if (NewSelection.IsValid())
    {
        CurrentFormatPreference = *NewSelection;
        SaveFormatPreference();

        FString FormatName = GetCurrentFormatText().ToString();
        AddSystemMessage(FString::Printf(TEXT("输出格式已切换为: %s"), *FormatName));
    }
}

FText SAIChatWindow::GetCurrentFormatText() const
{
    switch (CurrentFormatPreference)
    {
    case EModelFormatPreference::Default: return LOCTEXT("FormatDefault", "默认");
    case EModelFormatPreference::GLB:    return LOCTEXT("FormatGLB", "GLB");
    case EModelFormatPreference::OBJ:    return LOCTEXT("FormatOBJ", "OBJ");
    case EModelFormatPreference::STL:    return LOCTEXT("FormatSTL", "STL");
    case EModelFormatPreference::USDZ:   return LOCTEXT("FormatUSDZ", "USDZ");
    case EModelFormatPreference::FBX:    return LOCTEXT("FormatFBX", "FBX");
    default:                             return LOCTEXT("FormatUnknown", "未知");
    }
}

FText SAIChatWindow::GetFormatDescription(EModelFormatPreference Format) const
{
    switch (Format)
    {
    case EModelFormatPreference::Default: return LOCTEXT("DescDefault", "同时返回OBJ和GLB格式");
    case EModelFormatPreference::GLB:     return LOCTEXT("DescGLB", "单个文件，支持PBR材质");
    case EModelFormatPreference::OBJ:     return LOCTEXT("DescOBJ", "ZIP压缩包，包含MTL和纹理");
    case EModelFormatPreference::STL:     return LOCTEXT("DescSTL", "仅几何数据，适合3D打印");
    case EModelFormatPreference::USDZ:    return LOCTEXT("DescUSDZ", "苹果AR格式");
    case EModelFormatPreference::FBX:     return LOCTEXT("DescFBX", "支持动画，适合游戏引擎");
    default:                              return FText::GetEmpty();
    }
}

EModelFormat SAIChatWindow::ConvertToAPIModelFormat(EModelFormatPreference Preference) const
{
    switch (Preference)
    {
    case EModelFormatPreference::Default: return EModelFormat::Default;
    case EModelFormatPreference::GLB:     return EModelFormat::GLB;
    case EModelFormatPreference::OBJ:     return EModelFormat::OBJ;
    case EModelFormatPreference::STL:     return EModelFormat::STL;
    case EModelFormatPreference::USDZ:    return EModelFormat::USDZ;
    case EModelFormatPreference::FBX:     return EModelFormat::FBX;
    default:                              return EModelFormat::Default;
    }
}

void SAIChatWindow::SaveFormatPreference()
{
    auto Config = FHunYuanConfigManager::Get()->GetConfig();
    Config.LastUsedFormat = static_cast<EConfigFormatPreference>(CurrentFormatPreference);
    FHunYuanConfigManager::Get()->UpdateConfig(Config);
}

void SAIChatWindow::LoadFormatPreference()
{
    int32 SavedFormat = static_cast<int32>(EModelFormatPreference::Default);
    GConfig->GetInt(TEXT("HunYuanAI"), TEXT("FormatPreference"), SavedFormat, GEditorPerProjectIni);

    CurrentFormatPreference = (SavedFormat >= 0 && SavedFormat < FormatOptions.Num())
        ? static_cast<EModelFormatPreference>(SavedFormat)
        : EModelFormatPreference::Default;
}

// ==================== 生成任务 ====================

void SAIChatWindow::SubmitGenerationTask()
{
    FString Prompt = InputTextBox->GetText().ToString().TrimStartAndEnd();

    // 验证输入
    if (Prompt.IsEmpty() && !bUseMultiViewMode && CurrentImagePath.IsEmpty())
    {
        AddErrorMessage(TEXT("请输入文字描述或上传图片"));
        return;
    }

    if (bUseMultiViewMode)
    {
        if (GetValidViewCount() == 0)
        {
            AddErrorMessage(TEXT("请至少上传一张正视图图片"));
            return;
        }

        if (!IsViewImageValid(EViewType::Front))
        {
            AddErrorMessage(TEXT("多视图模式下必须包含正视图"));
            return;
        }
    }

    // 检查API是否就绪
    auto Config = FHunYuanConfigManager::Get()->GetConfig();
    if (!Config.HasValidCredentials())
    {
        AddErrorMessage(TEXT("请先配置API凭证"));
        return;
    }

    // 添加用户消息
    FString UserMessage = Prompt;
    if (bUseMultiViewMode)
    {
        UserMessage += FString::Printf(TEXT(" [多视图模式: %d张图片]"), GetValidViewCount());
    }
    else if (!CurrentImagePath.IsEmpty())
    {
        UserMessage += FString::Printf(TEXT(" [图片: %s]"), *FPaths::GetCleanFilename(CurrentImagePath));
    }
    AddUserMessage(UserMessage);

    // 清空输入
    InputTextBox->SetText(FText::GetEmpty());

    // 创建新任务
    CurrentTask = Chat::FGenerationTask();
    CurrentTask.Prompt = Prompt;
    CurrentTask.ImagePath = CurrentImagePath;
    CurrentTask.Status = Chat::EGenerationStatus::Submitting;
    CurrentTask.StartTime = FDateTime::Now();

    // 显示进度
    ProgressContainer->SetVisibility(EVisibility::Visible);
    UpdateProgress(CurrentTask);

    // 根据模式提交任务
    if (bUseMultiViewMode)
    {
        // 创建多视图输入
        HunYuanAPI::FMultiViewInput MultiViewInput;

        // 遍历所有视图，收集有效的图片
        for (auto& Pair : ViewImages)
        {
            UE_LOG(LogTemp, Log, TEXT("检查视图 %d: bIsLoaded=%d, Base64长度=%d, 文件路径=%s"),
                (int32)Pair.Key, Pair.Value.bIsLoaded, Pair.Value.Base64Data.Len(), *Pair.Value.FilePath);

            if (Pair.Value.bIsLoaded && !Pair.Value.Base64Data.IsEmpty())
            {
                // 创建图片信息
                HunYuanAPI::FMultiViewImage ImageInfo;
                ImageInfo.ViewType = Pair.Key;
                ImageInfo.FilePath = Pair.Value.FilePath;
                ImageInfo.Base64Data = Pair.Value.Base64Data;
                ImageInfo.bIsValid = true;

                // 添加到输入中
                MultiViewInput.Images.Add(ImageInfo);

                UE_LOG(LogTemp, Log, TEXT("成功添加图片: ViewType=%s, Base64长度=%d"),
                    *ImageInfo.GetViewTypeString(), ImageInfo.Base64Data.Len());
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("跳过无效视图 %d: bIsLoaded=%d, Base64长度=%d"),
                    (int32)Pair.Key, Pair.Value.bIsLoaded, Pair.Value.Base64Data.Len());
            }
        }

        // 验证是否有有效的图片
        if (MultiViewInput.Images.Num() == 0)
        {
            AddErrorMessage(TEXT("没有有效的多视图图片，请重新上传"));
            UE_LOG(LogTemp, Error, TEXT("提交失败：MultiViewInput.Images 为空"));
            return;
        }

        UE_LOG(LogTemp, Warning, TEXT("准备提交多视图任务，共 %d 张图片"), MultiViewInput.Images.Num());

        // 提交多视图任务
        SubmitMultiViewTo3D(MultiViewInput);
    }
    else if (!CurrentImagePath.IsEmpty())
    {
        SubmitImageTo3D(CurrentImagePath);
    }
    else
    {
        SubmitTextTo3D(Prompt);
    }
}

FReply SAIChatWindow::OnSendButtonClicked()
{
    SubmitGenerationTask();
    return FReply::Handled();
}

void SAIChatWindow::OnInputTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
    if (CommitType == ETextCommit::OnEnter)
    {
        OnSendButtonClicked();
    }
}

void SAIChatWindow::SubmitTextTo3D(const FString& Prompt)
{
    AddSystemMessage(TEXT("正在提交文生3D任务..."));

    FHunYuanAPI::Get()->SubmitProJobFromText(
        Prompt,
        ConvertToAPIModelFormat(CurrentFormatPreference),
        FOnJobSubmitted::CreateSP(this, &SAIChatWindow::OnJobSubmitted));
}

void SAIChatWindow::SubmitImageTo3D(const FString& ImagePath)
{
    AddSystemMessage(TEXT("正在提交图生3D任务..."));

    TArray<uint8> ImageData;
    if (!FFileHelper::LoadFileToArray(ImageData, *ImagePath))
    {
        AddErrorMessage(TEXT("图片加载失败"));
        ClearProgress();
        return;
    }

    FHunYuanAPI::Get()->SubmitProJobFromImage(
        ImageData,
        ConvertToAPIModelFormat(CurrentFormatPreference),
        FOnJobSubmitted::CreateSP(this, &SAIChatWindow::OnJobSubmitted));
}

void SAIChatWindow::OnJobSubmitted(bool bSuccess, const FString& JobIdOrError)
{
    if (bSuccess)
    {
        CurrentTask.JobId = JobIdOrError;
        CurrentTask.Status = Chat::EGenerationStatus::Waiting;

        AddSystemMessage(FString::Printf(TEXT("任务提交成功！JobId: %s"), *JobIdOrError));
        UpdateProgress(CurrentTask);

        PollJobResult(JobIdOrError);
    }
    else
    {
        CurrentTask.Status = Chat::EGenerationStatus::Failed;
        AddErrorMessage(TEXT("任务提交失败：") + JobIdOrError);
        ClearProgress();
    }
}

void SAIChatWindow::PollJobResult(const FString& JobId)
{
    FHunYuanAPI::Get()->QueryProJobResult(
        JobId,
        FOnJobQueried::CreateSP(this, &SAIChatWindow::OnJobQueried));
}

void SAIChatWindow::OnJobQueried(bool bSuccess, const TSharedPtr<FJsonObject>& Result)
{
    if (!bSuccess || !Result.IsValid())
    {
        // 继续轮询
        if (CurrentTask.Status == Chat::EGenerationStatus::Waiting)
        {
            TWeakPtr<SAIChatWindow> WeakThisPtr = SharedThis(this);
            FString JobId = CurrentTask.JobId;

            FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateLambda([WeakThisPtr, JobId](float) -> bool
                    {
                        if (auto SharedThis = WeakThisPtr.Pin())
                        {
                            SharedThis->PollJobResult(JobId);
                        }
                        return false;
                    }),
                3.0f);
        }
        return;
    }

    // 解析JSON
    HunYuanAPI::FJobResult JobResult;
    if (!JobResult.ParseFromJson(Result))
    {
        AddErrorMessage(TEXT("解析任务结果失败"));
        ClearProgress();
        return;
    }

    if (JobResult.IsCompleted())
    {
        CurrentTask.Result = MakeShareable(new HunYuanAPI::FJobResult(JobResult));
        CurrentTask.Status = Chat::EGenerationStatus::Downloading;
        UpdateProgress(CurrentTask);

        if (JobResult.ModelFiles.Num() > 0)
        {
            FString SelectedUrl;
            FString SelectedFormat;
            FString TargetFormat;

            // 将用户偏好转换为目标格式字符串
            switch (CurrentFormatPreference)
            {
            case EModelFormatPreference::Default: TargetFormat = TEXT("GLB"); break;
            case EModelFormatPreference::GLB:     TargetFormat = TEXT("GLB"); break;
            case EModelFormatPreference::OBJ:     TargetFormat = TEXT("OBJ"); break;
            case EModelFormatPreference::STL:     TargetFormat = TEXT("STL"); break;
            case EModelFormatPreference::USDZ:    TargetFormat = TEXT("USDZ"); break;
            case EModelFormatPreference::FBX:     TargetFormat = TEXT("FBX"); break;
            default:                              TargetFormat = TEXT("GLB"); break;
            }

            // 查找用户指定的格式
            SelectedUrl = JobResult.GetModelUrlByFormat(TargetFormat);

            // 如果没找到，按优先级降级选择
            if (SelectedUrl.IsEmpty())
            {
                TArray<FString> PriorityList;
                if (TargetFormat == TEXT("FBX"))
                {
                    PriorityList = { TEXT("GLB"), TEXT("OBJ"), TEXT("STL"), TEXT("USDZ") };
                }
                else if (TargetFormat == TEXT("GLB"))
                {
                    PriorityList = { TEXT("OBJ"), TEXT("FBX"), TEXT("STL"), TEXT("USDZ") };
                }
                else if (TargetFormat == TEXT("OBJ"))
                {
                    PriorityList = { TEXT("GLB"), TEXT("FBX"), TEXT("STL"), TEXT("USDZ") };
                }
                else
                {
                    PriorityList = { TEXT("GLB"), TEXT("OBJ"), TEXT("FBX"), TEXT("STL"), TEXT("USDZ") };
                }

                for (const FString& Format : PriorityList)
                {
                    SelectedUrl = JobResult.GetModelUrlByFormat(Format);
                    if (!SelectedUrl.IsEmpty())
                    {
                        SelectedFormat = Format;
                        break;
                    }
                }
            }
            else
            {
                SelectedFormat = TargetFormat;
            }

            // 最后选择第一个可用格式
            if (SelectedUrl.IsEmpty() && JobResult.ModelFiles.Num() > 0)
            {
                SelectedUrl = JobResult.ModelFiles[0].Url;
                SelectedFormat = JobResult.ModelFiles[0].Format;
            }

            if (!SelectedUrl.IsEmpty())
            {
                TSharedPtr<IDownloadHandler> Handler = FDownloadHandlerFactory::GetHandler(SelectedUrl);

                // 通知用户格式降级
                if (SelectedFormat != TargetFormat)
                {
                    AddSystemMessage(FString::Printf(TEXT("选择的 %s 格式不可用，已自动降级为 %s 格式"),
                        *TargetFormat, *SelectedFormat));
                }

                AddSystemMessage(FString::Printf(TEXT("模型生成成功，开始下载 %s 格式..."), *SelectedFormat));

                auto Config = FHunYuanConfigManager::Get()->GetConfig();
                Downloader->AddDownload(
                    SelectedUrl,
                    CurrentTask.JobId,
                    Config.DownloadDirectory,
                    Handler,
                    FOnDownloadItemComplete::CreateSP(this, &SAIChatWindow::OnDownloadItemComplete));
            }
            else
            {
                AddErrorMessage(TEXT("未获取到模型下载地址"));
                ClearProgress();
            }
        }
        else
        {
            AddErrorMessage(TEXT("未获取到模型下载地址"));
            ClearProgress();
        }
    }
    else if (JobResult.IsFailed())
    {
        CurrentTask.Status = Chat::EGenerationStatus::Failed;
        AddErrorMessage(FString::Printf(TEXT("生成失败: %s"), *JobResult.ErrorMessage));
        ClearProgress();
    }
    else if (JobResult.IsProcessing())
    {
        // 继续轮询
        TWeakPtr<SAIChatWindow> WeakThisPtr = SharedThis(this);
        FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([WeakThisPtr, JobId = CurrentTask.JobId](float) -> bool
                {
                    if (auto SharedThis = WeakThisPtr.Pin())
                    {
                        SharedThis->PollJobResult(JobId);
                    }
                    return false;
                }),
            3.0f);
    }
}

// ==================== 下载处理 ====================

void SAIChatWindow::OnDownloadProgress(const Download::FDownloadItem& Item)
{
    if (Item.JobId == CurrentTask.JobId)
    {
        CurrentTask.Status = Chat::EGenerationStatus::Downloading;
        CurrentTask.Progress = Item.Progress;
        UpdateProgress(CurrentTask);
    }
}

void SAIChatWindow::OnDownloadItemComplete(const Download::FDownloadItem& Item)
{
    if (Item.JobId != CurrentTask.JobId) return;

    if (Item.Status == Download::EDownloadStatus::Completed)
    {
        TWeakPtr<SAIChatWindow> WeakThisPtr = SharedThis(this);

        // 获取 Handler（已在工厂中初始化）
        TSharedPtr<IDownloadHandler> Handler = FDownloadHandlerFactory::GetHandler(Item.URL);

        Async(EAsyncExecution::ThreadPool, [WeakThisPtr, FilePath = Item.DestinationPath,
            JobId = Item.JobId, Handler]()
            {
                if (!WeakThisPtr.IsValid()) return;

                FModelInfo NewInfo;
                bool bSuccess = false;

                // 使用 Handler 处理文件
                if (Handler.IsValid())
                {
                    UE_LOG(LogHunYuanDownload, Log, TEXT("Processing with handler: %s"), *Handler->GetFormatName());
                    bSuccess = Handler->ProcessDownloadedFile(FilePath, JobId, NewInfo);
                }
                else
                {
                    // 降级处理：直接使用默认逻辑
                    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
                    if (PlatformFile.FileExists(*FilePath))
                    {
                        NewInfo.FilePath = FilePath;
                        NewInfo.FileName = FPaths::GetCleanFilename(FilePath);
                        NewInfo.Format = FPaths::GetExtension(FilePath).ToUpper();
                        NewInfo.FileSize = PlatformFile.FileSize(*FilePath);
                        NewInfo.JobId = JobId;
                        bSuccess = true;
                    }
                }

                AsyncTask(ENamedThreads::GameThread, [WeakThisPtr, NewInfo, bSuccess]()
                    {
                        if (auto SharedThis = WeakThisPtr.Pin())
                        {
                            SharedThis->FinishDownloadProcessing(NewInfo, bSuccess);
                        }
                    });
            });
    }
    else
    {
        // 错误处理保持不变
        TWeakPtr<SAIChatWindow> WeakThisPtr = SharedThis(this);
        AsyncTask(ENamedThreads::GameThread, [WeakThisPtr, Item]()
            {
                if (auto SharedThis = WeakThisPtr.Pin())
                {
                    SharedThis->CurrentTask.Status = Chat::EGenerationStatus::Failed;
                    SharedThis->AddErrorMessage(TEXT("下载失败: ") + Item.ErrorMessage);
                    SharedThis->ClearProgress();
                }
            });
    }
}

void SAIChatWindow::FinishDownloadProcessing(const FModelInfo& ModelInfo, bool bSuccess)
{
    if (!bSuccess)
    {
        AddErrorMessage(TEXT("文件处理失败"));
        ClearProgress();
        return;
    }

    AddToHistory(ModelInfo);

    CurrentTask.Status = Chat::EGenerationStatus::Completed;
    CurrentTask.EndTime = FDateTime::Now();

    AddSystemMessage(FString::Printf(TEXT("生成完成！耗时: %.1f秒"), CurrentTask.GetElapsedTime()));
    ClearProgress();

    auto Config = FHunYuanConfigManager::Get()->GetConfig();

    if (Config.bShowPreviewAfterDownload)
    {
        ShowModelPreview(ModelInfo);
    }

    if (Config.bAutoImport)
    {
        FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([this, ModelInfo](float) -> bool
                {
                    ImportModel(ModelInfo);
                    return false;
                }),
            0.1f);
    }
}

void SAIChatWindow::HandleDownloadedFile(const FString& FilePath, const FString& JobId)
{
    // 此函数已被 FinishDownloadProcessing 替代，保留以防其他地方调用
}

// ==================== 模型处理 ====================

void SAIChatWindow::ImportModel(const FModelInfo& ModelInfo)
{
    FString TargetPath = FPaths::ConvertRelativePathToFull(ModelInfo.FilePath).Replace(TEXT("\\"), TEXT("/"));
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    FString ImportSourcePath;
    FString DestinationSubPath;
    bool bIsZipBased = false;
    FString Format = ModelInfo.Format.IsEmpty() ? FPaths::GetExtension(ModelInfo.FileName).ToUpper() : ModelInfo.Format;

    // 根据格式处理
    if (Format == TEXT("OBJ") || Format == TEXT("ZIP") || TargetPath.EndsWith(TEXT(".zip"), ESearchCase::IgnoreCase))
    {
        // OBJ/ZIP 格式
        bIsZipBased = true;

        if (PlatformFile.FileExists(*TargetPath) && TargetPath.EndsWith(TEXT(".zip"), ESearchCase::IgnoreCase))
        {
            ImportSourcePath = FPaths::GetPath(TargetPath) / FPaths::GetBaseFilename(TargetPath);
            ImportSourcePath = ImportSourcePath.Replace(TEXT("\\"), TEXT("/"));
        }
        else if (PlatformFile.DirectoryExists(*TargetPath))
        {
            ImportSourcePath = TargetPath;
        }
        else
        {
            ShowImportErrorNotification(TEXT("无效的OBJ/ZIP路径"));
            return;
        }

        if (!PlatformFile.DirectoryExists(*ImportSourcePath))
        {
            ShowImportErrorNotification(TEXT("解压目录不存在"));
            return;
        }

        // 查找OBJ文件
        TArray<FString> FoundFiles;
        PlatformFile.FindFilesRecursively(FoundFiles, *ImportSourcePath, TEXT("*.obj"));
        if (FoundFiles.Num() == 0)
        {
            PlatformFile.FindFilesRecursively(FoundFiles, *ImportSourcePath, TEXT("*.OBJ"));
        }

        if (FoundFiles.Num() > 0)
        {
            ImportSourcePath = FPaths::GetPath(FoundFiles[0]);
        }
        else
        {
            ShowImportErrorNotification(TEXT("未找到OBJ文件"));
            return;
        }

        DestinationSubPath = FPaths::GetBaseFilename(ModelInfo.FileName);
    }
    else if (Format == TEXT("FBX") || Format == TEXT("GLB") || Format == TEXT("GLTF") ||
        Format == TEXT("STL") || Format == TEXT("USDZ"))
    {
        // 单文件格式
        bIsZipBased = false;

        if (!PlatformFile.FileExists(*TargetPath))
        {
            ShowImportErrorNotification(TEXT("文件不存在"));
            return;
        }

        ImportSourcePath = TargetPath;
    }
    else
    {
        ShowImportErrorNotification(FString::Printf(TEXT("不支持的格式: %s"), *Format));
        return;
    }

    // 调用导入管理器
    auto LocalImportManager = FModelImportManager::Get();
    if (LocalImportManager.IsValid())
    {
        LocalImportManager->OnModelImported.AddSP(this, &SAIChatWindow::HandleModelImported);

        FString DestinationPath = bIsZipBased
            ? FString::Printf(TEXT("/Game/HunyuanImports/%s/"), *DestinationSubPath)
            : TEXT("/Game/HunyuanImports/");

        bool bSuccess = false;

        if (bIsZipBased)
        {
            bSuccess = LocalImportManager->ImportModelFromFolder(ImportSourcePath, DestinationPath);
        }
        else
        {
            TArray<UObject*> ImportedAssets;
            bSuccess = (LocalImportManager->ImportModelInternal(ImportSourcePath, DestinationPath, ImportedAssets) == EModelImportResult::Success);

            if (bSuccess && ImportedAssets.Num() > 0)
            {
                FString FinalAssetPath = DestinationPath + FPaths::GetBaseFilename(ModelInfo.FileName);
                HandleModelImported(true, FinalAssetPath);
            }
            else
            {
                HandleModelImported(false, FString());
            }
        }

        if (bSuccess)
        {
            FSlateNotificationManager::Get().AddNotification(FNotificationInfo(
                FText::Format(NSLOCTEXT("AIChat", "ImportStarted", "开始导入模型: {0}"),
                    FText::FromString(ModelInfo.FileName))));

            if (ModelInfoWindow.IsValid())
            {
                ModelInfoWindow->RequestDestroyWindow();
                ModelInfoWindow.Reset();
            }
        }
    }
    else
    {
        ShowImportErrorNotification(TEXT("导入管理器初始化失败"));
    }
}

void SAIChatWindow::ShowImportErrorNotification(const FString& ErrorMessage)
{
    FNotificationInfo Info(FText::Format(
        NSLOCTEXT("AIChat", "ImportError", "导入失败: {0}"),
        FText::FromString(ErrorMessage)));
    Info.ExpireDuration = 3.0f;
    Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Error"));
    FSlateNotificationManager::Get().AddNotification(Info);
}

void SAIChatWindow::ShowModelPreview(const FModelInfo& ModelInfo)
{
    // 确保在游戏线程
    if (!IsInGameThread())
    {
        AsyncTask(ENamedThreads::GameThread, [this, ModelInfo]()
            {
                ShowModelPreview(ModelInfo);
            });
        return;
    }

    if (ModelInfoWindow.IsValid())
    {
        ModelInfoWindow->RequestDestroyWindow();
    }

    if (!FSlateApplication::IsInitialized())
    {
        return;
    }

    ModelInfoWindow = SNew(SWindow)
        .Title(FText::Format(LOCTEXT("ModelPreview", "模型预览 - {0}"), FText::FromString(ModelInfo.FileName)))
        .ClientSize(FVector2D(600, 500))
        .SupportsMaximize(false)
        .SupportsMinimize(false)
        .SizingRule(ESizingRule::UserSized)
        .AutoCenter(EAutoCenter::PreferredWorkArea)
        .IsInitiallyMaximized(false)
        .FocusWhenFirstShown(true)
        .CreateTitleBar(true);

    if (!ModelInfoWindow.IsValid())
    {
        return;
    }

    ModelInfoWindow->SetContent(
        SNew(SModelInfoWidget)
        .ModelInfo(ModelInfo)
        .OnImportClicked(FOnModelAction::CreateSP(this, &SAIChatWindow::ImportModel))
        .OnPreviewClicked(FOnModelAction::CreateSP(this, &SAIChatWindow::PreviewModel))
        .OnDeleteClicked(FOnModelAction::CreateSP(this, &SAIChatWindow::DeleteModelFile))
        .OnOpenFolderClicked(FOnModelAction::CreateSP(this, &SAIChatWindow::OpenModelFolder)));

    FSlateApplication::Get().AddWindow(ModelInfoWindow.ToSharedRef());
}

void SAIChatWindow::PreviewModel(const FModelInfo& ModelInfo)
{
    FSlateNotificationManager::Get().AddNotification(FNotificationInfo(
        FText::Format(NSLOCTEXT("AIChat", "PreviewNotImplemented", "预览功能开发中: {0}"),
            FText::FromString(ModelInfo.FileName))));
}

void SAIChatWindow::DeleteModelFile(const FModelInfo& ModelInfo)
{
    // 确认对话框
    if (FMessageDialog::Open(EAppMsgType::YesNo,
        FText::Format(NSLOCTEXT("AIChat", "ConfirmDelete",
            "确定要删除模型 \"{0}\" 吗？\n\n文件: {1}\n大小: {2}\n\n此操作不可撤销！"),
            FText::FromString(ModelInfo.FileName),
            FText::FromString(ModelInfo.FilePath),
            FText::FromString(ModelInfo.GetFileSizeString()))) != EAppReturnType::Yes)
    {
        return;
    }

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    bool bDeleted = false;

    if (PlatformFile.FileExists(*ModelInfo.FilePath))
    {
        bDeleted = PlatformFile.DeleteFile(*ModelInfo.FilePath);
    }

    if (bDeleted)
    {
        FString FolderPath = FPaths::GetPath(ModelInfo.FilePath);
        FString BaseFilename = FPaths::GetBaseFilename(ModelInfo.FilePath);

        // 删除 MTL 文件
        FString MtlPath = FolderPath / BaseFilename + TEXT(".mtl");
        if (PlatformFile.FileExists(*MtlPath))
        {
            PlatformFile.DeleteFile(*MtlPath);
        }

        // 删除纹理文件
        TArray<FString> TextureExtensions = { TEXT(".png"), TEXT(".jpg"), TEXT(".jpeg"), TEXT(".tga"), TEXT(".bmp"), TEXT(".dds") };
        for (const FString& Ext : TextureExtensions)
        {
            FString TexturePath = FolderPath / BaseFilename + Ext;
            if (PlatformFile.FileExists(*TexturePath))
            {
                PlatformFile.DeleteFile(*TexturePath);
                break;
            }
        }

        FSlateNotificationManager::Get().AddNotification(FNotificationInfo(
            FText::Format(NSLOCTEXT("AIChat", "DeleteSuccess", "已删除: {0}"),
                FText::FromString(ModelInfo.FileName))));

        RefreshModelList();

        if (ModelInfoWindow.IsValid())
        {
            ModelInfoWindow->RequestDestroyWindow();
            ModelInfoWindow.Reset();
        }
    }
}

void SAIChatWindow::OpenModelFolder(const FModelInfo& ModelInfo)
{
    FString FolderPath = FPaths::GetPath(ModelInfo.FilePath);
    FString ExtractedFolderPath = FolderPath / FPaths::GetBaseFilename(ModelInfo.FilePath);
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    if (PlatformFile.DirectoryExists(*ExtractedFolderPath))
    {
        FPlatformProcess::ExploreFolder(*ExtractedFolderPath);
    }
    else if (PlatformFile.DirectoryExists(*FolderPath))
    {
        FPlatformProcess::ExploreFolder(*FolderPath);
    }
    else
    {
        FSlateNotificationManager::Get().AddNotification(FNotificationInfo(
            FText::Format(NSLOCTEXT("AIChat", "FolderNotFound", "文件夹不存在: {0}"),
                FText::FromString(ModelInfo.FileName))));
    }
}

void SAIChatWindow::HandleModelImported(bool bSuccess, const FString& AssetPath)
{
    if (bSuccess)
    {
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        TArray<FAssetData> Assets;
        AssetRegistryModule.Get().GetAssetsByPath(FName(*AssetPath), Assets, true);

        FSlateNotificationManager::Get().AddNotification(FNotificationInfo(
            FText::Format(NSLOCTEXT("AIChat", "ImportSuccess", "模型导入成功: {0} (已导入 {1} 个资产)"),
                FText::FromString(FPaths::GetBaseFilename(AssetPath)),
                FText::AsNumber(Assets.Num()))));

        RefreshModelList();
    }
}

void SAIChatWindow::RefreshModelList()
{
    // TODO: 实现模型列表刷新逻辑
}

// ==================== 历史管理 ====================

void SAIChatWindow::LoadDownloadHistory()
{
    const FString HistoryFile = FPaths::ProjectSavedDir() / TEXT("HunYuanAI/DownloadHistory.json");
    if (!FPaths::FileExists(HistoryFile)) return;

    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *HistoryFile)) return;

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid()) return;

    DownloadHistory.Empty();
    const TArray<TSharedPtr<FJsonValue>>* HistoryArray;

    if (JsonObject->TryGetArrayField(TEXT("History"), HistoryArray))
    {
        for (const auto& Item : *HistoryArray)
        {
            TSharedPtr<FJsonObject> ItemObject = Item->AsObject();
            if (ItemObject.IsValid())
            {
                FModelInfo Info;
                Info.FilePath = ItemObject->GetStringField(TEXT("FilePath"));
                Info.FileName = ItemObject->GetStringField(TEXT("FileName"));
                Info.JobId = ItemObject->GetStringField(TEXT("JobId"));
                Info.URL = ItemObject->GetStringField(TEXT("URL"));
                Info.Format = ItemObject->GetStringField(TEXT("Format"));
                Info.FileSize = static_cast<int64>(ItemObject->GetNumberField(TEXT("FileSize")));
                FDateTime::Parse(ItemObject->GetStringField(TEXT("DownloadTime")), Info.DownloadTime);

                if (FPaths::FileExists(Info.FilePath))
                {
                    DownloadHistory.Add(Info);
                }
            }
        }
    }
}

void SAIChatWindow::SaveDownloadHistory()
{
    const FString HistoryFile = FPaths::ProjectSavedDir() / TEXT("HunYuanAI/DownloadHistory.json");
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    FString HistoryDir = FPaths::GetPath(HistoryFile);

    if (!PlatformFile.DirectoryExists(*HistoryDir))
    {
        PlatformFile.CreateDirectoryTree(*HistoryDir);
    }

    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject());
    TArray<TSharedPtr<FJsonValue>> HistoryArray;

    for (const auto& Info : DownloadHistory)
    {
        TSharedPtr<FJsonObject> ItemObject = MakeShareable(new FJsonObject());
        ItemObject->SetStringField(TEXT("FilePath"), Info.FilePath);
        ItemObject->SetStringField(TEXT("FileName"), Info.FileName);
        ItemObject->SetStringField(TEXT("JobId"), Info.JobId);
        ItemObject->SetStringField(TEXT("URL"), Info.URL);
        ItemObject->SetStringField(TEXT("Format"), Info.Format);
        ItemObject->SetNumberField(TEXT("FileSize"), Info.FileSize);
        ItemObject->SetStringField(TEXT("DownloadTime"), Info.DownloadTime.ToString());
        HistoryArray.Add(MakeShareable(new FJsonValueObject(ItemObject)));
    }

    RootObject->SetArrayField(TEXT("History"), HistoryArray);

    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);

    if (FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
    {
        FFileHelper::SaveStringToFile(JsonString, *HistoryFile);
    }
}

void SAIChatWindow::AddToHistory(const FModelInfo& ModelInfo)
{
    DownloadHistory.Add(ModelInfo);
    SaveDownloadHistory();
}

// ==================== UI状态更新 ====================

void SAIChatWindow::UpdateProgress(const Chat::FGenerationTask& Task)
{
    AsyncTask(ENamedThreads::GameThread, [this, Task]()
        {
            CurrentTask = Task;

            if (ProgressText.IsValid())
            {
                ProgressText->SetText(FText::FromString(Task.GetStatusText()));
            }

            if (ProgressBar.IsValid())
            {
                ProgressBar->SetPercent(Task.Progress);
            }
        });
}

void SAIChatWindow::ClearProgress()
{
    AsyncTask(ENamedThreads::GameThread, [this]()
        {
            ProgressContainer->SetVisibility(EVisibility::Collapsed);
            CurrentTask = Chat::FGenerationTask();
        });
}

TOptional<float> SAIChatWindow::GetProgressPercent() const
{
    return (CurrentTask.Status == Chat::EGenerationStatus::Waiting)
        ? TOptional<float>()
        : CurrentTask.Progress;
}

FReply SAIChatWindow::OnSelectDownloadDirectoryClicked()
{
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (DesktopPlatform)
    {
        FString OutFolder;
        if (DesktopPlatform->OpenDirectoryDialog(
            FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
            TEXT("选择下载目录"),
            FHunYuanConfigManager::Get()->GetConfig().DownloadDirectory,
            OutFolder) && !OutFolder.IsEmpty())
        {
            auto Config = FHunYuanConfigManager::Get()->GetConfig();
            Config.DownloadDirectory = OutFolder;
            FHunYuanConfigManager::Get()->UpdateConfig(Config);
        }
    }
    return FReply::Handled();
}

// ==================== 知识图谱搜索集成 ====================

void SAIChatWindow::AddKnowledgeGraphSearchPanel()
{
    // 创建知识图谱搜索组件
    KnowledgeGraphSearch = SNew(SKnowledgeGraphSearchWidget)
        .OnDocumentSelected(FOnDocumentSelected::CreateSP(this, &SAIChatWindow::OnSearchResultSelected));

    // 创建参考面板
    ReferencePanel = SNew(SBox)
        .Visibility(EVisibility::Collapsed)
        [
            SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
                .Padding(8)
                [
                    SNew(SVerticalBox)

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 0, 0, 4)
                        [
                            SNew(SHorizontalBox)

                                + SHorizontalBox::Slot()
                                .FillWidth(1.0f)
                                [
                                    SAssignNew(ReferenceTitleText, STextBlock)
                                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                                ]

                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                [
                                    SNew(SButton)
                                        .ButtonStyle(FCoreStyle::Get(), "NoBorder")
                                        .ToolTipText(LOCTEXT("CopyToPrompt", "复制到提示词输入框"))
                                        .OnClicked(this, &SAIChatWindow::OnCopyReferenceToPrompt)
                                        [
                                            SNew(STextBlock)
                                                .Text(FText::FromString(TEXT("📋")))
                                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                                        ]
                                ]
                        ]

                    + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 0, 0, 8)
                        [
                            SNew(SSeparator)
                        ]

                        + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        [
                            SNew(SScrollBox)
                                + SScrollBox::Slot()
                                [
                                    SAssignNew(ReferenceContentText, STextBlock)
                                        .AutoWrapText(true)
                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                ]
                        ]
                ]
        ];
}

TSharedRef<SWidget> SAIChatWindow::BuildMainPanel()
{
    return SNew(SScrollBox)
        + SScrollBox::Slot()
        .Padding(10)
        [
            SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(5)
                [
                    BuildApiConfigSection()
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(5)
                [
                    BuildImportOptionsSection()
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(5)
                [
                    BuildModelSelectionSection()
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(5)
                [
                    BuildFormatSelectionSection()
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(5)
                [
                    BuildImageInputSection()
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(5)
                [
                    BuildChatHistorySection()
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(5)
                [
                    BuildProgressSection()
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(5)
                [
                    BuildInputSection()
                ]
        ];
}

TSharedRef<SWidget> SAIChatWindow::BuildSidebar()
{
    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.DarkGroupBorder"))
        .Padding(0)
        [
            SNew(SVerticalBox)

                // 侧边栏标题栏
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(8, 8, 8, 4)
                [
                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Text(LOCTEXT("KnowledgeGraphSidebar", "📚 知识图谱"))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SButton)
                                .ButtonStyle(FCoreStyle::Get(), "NoBorder")
                                .ContentPadding(FMargin(4, 2))
                                .OnClicked(this, &SAIChatWindow::OnToggleSidebar)
                                .ToolTipText(LOCTEXT("ToggleSidebar", "折叠侧边栏"))
                                [
                                    SNew(STextBlock)
                                        .Text_Lambda([this]()
                                            {
                                                return FText::FromString(bSidebarVisible ? TEXT("▶") : TEXT("◀"));
                                            })
                                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                                ]
                        ]
                ]

            // 侧边栏内容
            + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SNew(SBox)
                        .Visibility_Lambda([this]()
                            {
                                return bSidebarVisible ? EVisibility::Visible : EVisibility::Collapsed;
                            })
                        [
                            SNew(SVerticalBox)

                                + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(8, 0, 8, 8)
                                [
                                    SNew(SSeparator)
                                        .Orientation(Orient_Horizontal)
                                ]

                                + SVerticalBox::Slot()
                                .FillHeight(0.6f)
                                .Padding(8, 0, 8, 8)
                                [
                                    KnowledgeGraphSearch.ToSharedRef()
                                ]

                                + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(8, 0, 8, 8)
                                [
                                    SNew(SSeparator)
                                        .Orientation(Orient_Horizontal)
                                ]

                                + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(8, 0, 8, 4)
                                [
                                    SNew(STextBlock)
                                        .Text(LOCTEXT("ReferenceInfo", "📖 参考信息"))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                                ]

                                + SVerticalBox::Slot()
                                .FillHeight(0.4f)
                                .Padding(8, 0, 8, 8)
                                [
                                    ReferencePanel.ToSharedRef()
                                ]
                        ]
                ]
        ];
}

FReply SAIChatWindow::OnToggleSidebar()
{
    bSidebarVisible = !bSidebarVisible;
    return FReply::Handled();
}

void SAIChatWindow::OnSidebarResized(float NewSize)
{
    if (NewSize > 0.01f)
    {
        CachedSidebarSize = NewSize;
        if (!bSidebarVisible && NewSize > 0.01f)
        {
            bSidebarVisible = true;
        }
    }
    else if (NewSize <= 0.01f)
    {
        bSidebarVisible = false;
    }
}

void SAIChatWindow::OnSearchResultSelected(const FSourceDocument& Document)
{
    ShowReferencePanel(Document);
    AddSystemMessage(FString::Printf(TEXT("已加载参考文档: %s，可参考其中信息编写提示词"), *Document.ResourceName));
}

void SAIChatWindow::ShowReferencePanel(const FSourceDocument& Document)
{
    if (!ReferencePanel.IsValid()) return;

    FString Title = FString::Printf(TEXT("%s (%s)"), *Document.ResourceName, *Document.ResourceType);
    ReferenceTitleText->SetText(FText::FromString(Title));

    FString Content = Document.Content;
    if (Content.Len() > 500)
    {
        Content = Content.Left(500) + TEXT("\n\n... (完整内容可在知识图谱窗口中查看)");
    }
    ReferenceContentText->SetText(FText::FromString(Content));

    ReferencePanel->SetVisibility(EVisibility::Visible);
}

void SAIChatWindow::HideReferencePanel()
{
    if (ReferencePanel.IsValid())
    {
        ReferencePanel->SetVisibility(EVisibility::Collapsed);
    }
}

FReply SAIChatWindow::OnCopyReferenceToPrompt()
{
    if (ReferenceContentText.IsValid() && InputTextBox.IsValid())
    {
        FString NewPrompt = InputTextBox->GetText().ToString();
        if (!NewPrompt.IsEmpty())
        {
            NewPrompt += TEXT("\n\n");
        }
        NewPrompt += TEXT("[参考信息]\n") + ReferenceContentText->GetText().ToString();

        InputTextBox->SetText(FText::FromString(NewPrompt));
        AddSystemMessage(TEXT("已复制参考信息到提示词输入框，您可以根据需要修改"));
        FSlateApplication::Get().SetKeyboardFocus(InputTextBox);
    }

    return FReply::Handled();
}

void SAIChatWindow::OnConfigChanged()
{
    UpdateCredentialsUI();
}

void SAIChatWindow::UpdateViewImagePreview(EViewType ViewType, UTexture2D* Texture,
    const FString& Base64Data, const FString& FileName, int64 FileSize)
{
    if (!Texture)
    {
        UE_LOG(LogTemp, Error, TEXT("UpdateViewImagePreview: 纹理为空"));
        return;
    }

    if (!ViewImages.Contains(ViewType))
    {
        UE_LOG(LogTemp, Error, TEXT("UpdateViewImagePreview: ViewType不存在"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("UpdateViewImagePreview: 更新视图 %d, 纹理=%p"), (int32)ViewType, Texture);

    FViewImageItem& Item = ViewImages[ViewType];

    // 创建或更新画笔
    if (!Item.Brush.IsValid())
    {
        Item.Brush = MakeShareable(new FSlateBrush());
        Item.Brush->DrawAs = ESlateBrushDrawType::Image;
    }

    // 设置纹理
    Item.Brush->SetResourceObject(Texture);
    Item.Brush->ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());

    // 更新图像控件
    if (Item.ImageWidget.IsValid())
    {
        Item.ImageWidget->SetImage(Item.Brush.Get());
    }

    // 更新数据
    Item.FilePath = FileName;
    Item.Base64Data = Base64Data;
    Item.bIsLoaded = true;

    UE_LOG(LogTemp, Log, TEXT("UpdateViewImagePreview: ViewType=%d, Base64长度=%d"),
        (int32)ViewType, Base64Data.Len());

    // 更新信息文本
    if (ViewInfoTexts.Contains(ViewType))
    {
        FString SizeString = FString::Printf(TEXT("%.1f KB"), FileSize / 1024.0f);
        ViewInfoTexts[ViewType]->SetText(FText::Format(
            LOCTEXT("ImageLoaded", "{0}\n{1}"),
            FText::FromString(FileName),
            FText::FromString(SizeString)));
        ViewInfoTexts[ViewType]->SetColorAndOpacity(FLinearColor::Green);
    }

    if (ViewClearButtons.Contains(ViewType))
    {
        ViewClearButtons[ViewType]->SetEnabled(true);
    }
}

// 创建纹理
UTexture2D* FAsyncImageLoadResult::CreateTexture(bool bUseThumbnail) const
{
    if (!IsInGameThread())
    {
        UE_LOG(LogTemp, Error, TEXT("CreateTexture: 必须在游戏线程调用"));
        return nullptr;
    }

    // 选择使用原图还是缩略图
    const TSharedPtr<TArray<uint8>>& DataToUse = bUseThumbnail ? ThumbnailData : RawData;
    int32 WidthToUse = bUseThumbnail ? ThumbnailWidth : Width;
    int32 HeightToUse = bUseThumbnail ? ThumbnailHeight : Height;

    if (!DataToUse.IsValid() || DataToUse->Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("CreateTexture: 数据无效"));
        return nullptr;
    }

    if (WidthToUse <= 0 || HeightToUse <= 0)
    {
        UE_LOG(LogTemp, Error, TEXT("CreateTexture: 无效尺寸 %dx%d"), WidthToUse, HeightToUse);
        return nullptr;
    }

    // 验证数据大小
    int32 ExpectedSize = WidthToUse * HeightToUse * 4;
    if (DataToUse->Num() != ExpectedSize)
    {
        UE_LOG(LogTemp, Error, TEXT("CreateTexture: 数据大小不匹配 期望=%d, 实际=%d"),
            ExpectedSize, DataToUse->Num());
        return nullptr;
    }

    // 创建纹理
    UTexture2D* Texture = UTexture2D::CreateTransient(WidthToUse, HeightToUse, PF_B8G8R8A8);
    if (!Texture)
    {
        UE_LOG(LogTemp, Error, TEXT("CreateTexture: 创建纹理失败"));
        return nullptr;
    }

    // 配置纹理
    Texture->CompressionSettings = TC_EditorIcon;
    Texture->SRGB = true;
    Texture->NeverStream = true;

    // 写入数据
    FTexturePlatformData* PlatformData = Texture->GetPlatformData();
    if (PlatformData && PlatformData->Mips.Num() > 0)
    {
        FTexture2DMipMap& Mip = PlatformData->Mips[0];
        void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
        if (Data)
        {
            FMemory::Memcpy(Data, DataToUse->GetData(), DataToUse->Num());
            Mip.BulkData.Unlock();
            Texture->UpdateResource();

            UE_LOG(LogTemp, Log, TEXT("CreateTexture: 创建%s成功: %dx%d, 数据大小=%d"),
                bUseThumbnail ? TEXT("缩略图") : TEXT("原图"),
                WidthToUse, HeightToUse, DataToUse->Num());

            return Texture;
        }
        Mip.BulkData.Unlock();
    }

    Texture->ConditionalBeginDestroy();
    return nullptr;
}

#undef LOCTEXT_NAMESPACE