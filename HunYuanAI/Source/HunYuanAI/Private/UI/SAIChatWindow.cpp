#include "UI/SAIChatWindow.h"
#include "HunYuanAI.h"
#include "Logging/HunYuanLogging.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "ImageUtils.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "SAIChatWindow"

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

    ChildSlot
        [
            SNew(SScrollBox)
                + SScrollBox::Slot()
                .Padding(10)
                [
                    SNew(SVerticalBox)

                        // API配置区域
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(5)
                        [
                            BuildApiConfigSection()
                        ]

                        // 导入选项区域
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(5)
                        [
                            BuildImportOptionsSection()
                        ]

                        // 模型选择区域
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(5)
                        [
                            BuildModelSelectionSection()
                        ]

                        // 图片上传区域
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(5)
                        [
                            BuildImageUploadSection()
                        ]

                        // 对话历史区域
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(5)
                        [
                            BuildChatHistorySection()
                        ]

                        // 进度区域
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(5)
                        [
                            BuildProgressSection()
                        ]

                        // 输入区域
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(5)
                        [
                            BuildInputSection()
                        ]
                ]
        ];

    // 加载配置
    LoadConfig();

    // 绑定事件
    Downloader->OnDownloadProgress.AddSP(this, &SAIChatWindow::OnDownloadProgress);
    Downloader->OnDownloadComplete.AddSP(this, &SAIChatWindow::OnDownloadComplete);

    // 加载历史
    LoadDownloadHistory();

    // 添加欢迎消息
    Chat::FChatMessage WelcomeMsg;
    WelcomeMsg.Type = Chat::EMessageType::System;
    WelcomeMsg.Sender = TEXT("系统");
    WelcomeMsg.Content = TEXT("欢迎使用混元3D生成工具！请输入文字描述或上传图片开始生成3D模型。");
    AddMessage(WelcomeMsg);
}

SAIChatWindow::~SAIChatWindow()
{
    // 取消所有下载
    if (Downloader.IsValid())
    {
        Downloader->CancelAllDownloads();
    }

    // 保存历史
    SaveDownloadHistory();

    // 关闭预览窗口
    if (ModelInfoWindow.IsValid())
    {
        ModelInfoWindow->RequestDestroyWindow();
    }
}

// ==================== UI 构建 ====================

TSharedRef<SWidget> SAIChatWindow::BuildApiConfigSection()
{
    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
        .BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f))
        .Padding(10)
        [
            SNew(SVerticalBox)

                // 标题
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 10)
                [
                    SNew(STextBlock)
                        .Text(LOCTEXT("ApiConfig", "API 配置"))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                ]

                // SecretId输入
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(5)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0, 0, 10, 0)
                        [
                            SNew(STextBlock)
                                .Text(LOCTEXT("SecretId", "SecretId:"))
                                .MinDesiredWidth(80)
                        ]
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        [
                            SAssignNew(SecretIdInput, SEditableTextBox)
                                .HintText(LOCTEXT("SecretIdHint", "输入SecretId"))
                        ]
                ]

            // SecretKey输入
            + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(5)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0, 0, 10, 0)
                        [
                            SNew(STextBlock)
                                .Text(LOCTEXT("SecretKey", "SecretKey:"))
                                .MinDesiredWidth(80)
                        ]
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        [
                            SAssignNew(SecretKeyInput, SEditableTextBox)
                                .HintText(LOCTEXT("SecretKeyHint", "输入SecretKey"))
                                .IsPassword(true)
                        ]
                ]

            // 记住密码和保存按钮
            + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(5)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SAssignNew(RememberPasswordCheckBox, SCheckBox)
                                [
                                    SNew(STextBlock)
                                        .Text(LOCTEXT("RememberPassword", "记住密码"))
                                ]
                        ]
                    + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .HAlign(HAlign_Right)
                        [
                            SNew(SButton)
                                .Text(LOCTEXT("SaveConfig", "保存配置"))
                                .OnClicked(this, &SAIChatWindow::OnSaveConfigButtonClicked)
                        ]
                ]
        ];
}

TSharedRef<SWidget> SAIChatWindow::BuildImportOptionsSection()
{
    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
        .BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f))
        .Padding(10)
        [
            SNew(SVerticalBox)

                // 标题
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 10)
                [
                    SNew(STextBlock)
                        .Text(LOCTEXT("ImportOptions", "导入选项"))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                ]

                // 自动导入
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(5)
                [
                    SAssignNew(AutoImportCheckBox, SCheckBox)
                        [
                            SNew(STextBlock)
                                .Text(LOCTEXT("AutoImport", "下载完成后自动导入到内容浏览器"))
                        ]
                ]

            // 显示预览
            + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(5)
                [
                    SAssignNew(ShowPreviewCheckBox, SCheckBox)
                        [
                            SNew(STextBlock)
                                .Text(LOCTEXT("ShowPreview", "下载完成后显示模型预览窗口"))
                        ]
                ]

            // 下载目录
            + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(5)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        [
                            SAssignNew(DownloadDirText, STextBlock)
                                .AutoWrapText(true)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        [
                            SNew(SButton)
                                .Text(LOCTEXT("SelectDirectory", "选择目录"))
                                .OnClicked(this, &SAIChatWindow::OnSelectDownloadDirectoryClicked)
                        ]
                ]
        ];
}

TSharedRef<SWidget> SAIChatWindow::BuildModelSelectionSection()
{
    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
        .BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f))
        .Padding(10)
        [
            SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0, 0, 10, 0)
                [
                    SNew(STextBlock)
                        .Text(LOCTEXT("SelectModel", "选择模型:"))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SComboBox<TSharedPtr<FModelOption>>)
                        .OptionsSource(&ModelOptions)
                        .InitiallySelectedItem(CurrentSelectedModel)
                        .OnGenerateWidget(this, &SAIChatWindow::GenerateModelOptionWidget)
                        .OnSelectionChanged(this, &SAIChatWindow::OnModelSelectionChanged)
                        .Content()
                        [
                            SNew(STextBlock)
                                .Text(this, &SAIChatWindow::GetCurrentModelText)
                        ]
                ]
        ];
}

TSharedRef<SWidget> SAIChatWindow::BuildImageUploadSection()
{
    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
        .BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f))
        .Padding(10)
        [
            SNew(SVerticalBox)

                // 预览区域
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Center)
                [
                    SNew(SBox)
                        .WidthOverride(200)
                        .HeightOverride(200)
                        [
                            SAssignNew(ImagePreviewWidget, SImage)
                                .Image(ImageBrush.Get())
                        ]
                ]

            // 上传进度
            + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(5)
                [
                    SAssignNew(UploadProgressBar, SProgressBar)
                        .Visibility(EVisibility::Collapsed)
                ]

                // 按钮区域
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(5)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(5)
                        [
                            SNew(SButton)
                                .Text(LOCTEXT("UploadImage", "上传图片"))
                                .OnClicked(this, &SAIChatWindow::OnUploadImageButtonClicked)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(5)
                        [
                            SNew(SButton)
                                .Text(LOCTEXT("ClearImage", "清除图片"))
                                .OnClicked(this, &SAIChatWindow::OnClearImageButtonClicked)
                                .IsEnabled_Lambda([this]() { return !CurrentImagePath.IsEmpty(); })
                        ]
                ]

            // 图片信息
            + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(5)
                [
                    SAssignNew(ImageInfoText, STextBlock)
                        .Text(LOCTEXT("NoImage", "未选择图片"))
                        .ColorAndOpacity(FLinearColor::Gray)
                ]
        ];
}

TSharedRef<SWidget> SAIChatWindow::BuildChatHistorySection()
{
    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
        .BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f))
        .Padding(5)
        [
            SAssignNew(MessagesListView, SListView<TSharedPtr<Chat::FChatMessage>>)
                .ListItemsSource(&Messages)
                .OnGenerateRow(this, &SAIChatWindow::GenerateMessageRow)
                .ItemHeight(24)
        ];
}

TSharedRef<SWidget> SAIChatWindow::BuildProgressSection()
{
    return SAssignNew(ProgressContainer, SBorder)
        .Visibility(EVisibility::Collapsed)
        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
        .BorderBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f, 1.0f))
        .Padding(10)
        [
            SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 5)
                [
                    SAssignNew(ProgressText, STextBlock)
                        .Text(LOCTEXT("Generating", "生成中..."))
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SAssignNew(ProgressBar, SProgressBar)
                        .Percent(this, &SAIChatWindow::GetProgressPercent)
                ]
        ];
}

TSharedRef<SWidget> SAIChatWindow::BuildInputSection()
{
    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .Padding(5)
        [
            SAssignNew(InputTextBox, SEditableTextBox)
                .HintText(LOCTEXT("InputHint", "请描述你想生成的内容..."))
                .OnTextCommitted(this, &SAIChatWindow::OnInputTextCommitted)
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(5)
        [
            SNew(SButton)
                .Text(LOCTEXT("Send", "发送"))
                .OnClicked(this, &SAIChatWindow::OnSendButtonClicked)
                .IsEnabled_Lambda([this]() {
                return CurrentTask.Status == Chat::EGenerationStatus::Idle;
                    })
        ];
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
    }

    return SNew(STableRow<TSharedPtr<Chat::FChatMessage>>, OwnerTable)
        [
            SNew(STextBlock)
                .Text(Item->GetDisplayText())
                .AutoWrapText(true)
                .ColorAndOpacity(TextColor)
        ];
}

// ==================== 图片上传 ====================

FReply SAIChatWindow::OnUploadImageButtonClicked()
{
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (DesktopPlatform)
    {
        TArray<FString> OutFiles;
        const FString FileTypes = TEXT("图片文件|*.jpg;*.jpeg;*.png;*.bmp|所有文件|*.*");

        if (DesktopPlatform->OpenFileDialog(
            FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
            TEXT("选择图片"),
            FPaths::ProjectContentDir(),
            TEXT(""),
            FileTypes,
            EFileDialogFlags::None,
            OutFiles))
        {
            if (OutFiles.Num() > 0)
            {
                HandleUploadedImage(OutFiles[0]);
            }
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

    // 更新图片信息
    FString FileName = FPaths::GetCleanFilename(ImagePath);
    int64 FileSize = IFileManager::Get().FileSize(*ImagePath);
    FString SizeStr = FString::Printf(TEXT("%.1f KB"), FileSize / 1024.0f);

    ImageInfoText->SetText(FText::Format(
        LOCTEXT("ImageSelected", "已选择: {0} ({1})"),
        FText::FromString(FileName),
        FText::FromString(SizeStr)
    ));

    // 加载预览
    LoadImagePreview(ImagePath);
}

bool SAIChatWindow::LoadImagePreview(const FString& ImagePath)
{
    // 读取文件数据
    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *ImagePath))
    {
        UE_LOG(LogHunYuanUI, Error, TEXT("Failed to load image file: %s"), *ImagePath);
        return false;
    }

    // 获取 ImageWrapper 模块
    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

    // 检测图片格式
    EImageFormat Format = ImageWrapperModule.DetectImageFormat(FileData.GetData(), FileData.Num());
    if (Format == EImageFormat::Invalid)
    {
        UE_LOG(LogHunYuanUI, Error, TEXT("Unsupported image format: %s"), *ImagePath);
        return false;
    }

    // 创建 ImageWrapper
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(Format);
    if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
    {
        UE_LOG(LogHunYuanUI, Error, TEXT("Failed to set compressed data: %s"), *ImagePath);
        return false;
    }

    // 获取原始数据
    TArray<uint8> RawData;
    if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
    {
        UE_LOG(LogHunYuanUI, Error, TEXT("Failed to get raw data: %s"), *ImagePath);
        return false;
    }

    // 创建纹理
    UTexture2D* Texture = UTexture2D::CreateTransient(ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), PF_B8G8R8A8);
    if (!Texture)
    {
        UE_LOG(LogHunYuanUI, Error, TEXT("Failed to create transient texture"));
        return false;
    }

    // 锁定纹理并填充数据
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

    // 绑定配置变更事件
    ConfigManager->OnConfigChanged.AddSP(this, &SAIChatWindow::OnConfigChanged);

    // 设置API凭证
    if (Config.HasValidCredentials())
    {
        FHunYuanAPI::Get()->SetCredentials(Config.SecretId, Config.SecretKey, Config.Region);
        UE_LOG(LogHunYuanUI, Log, TEXT("API credentials set from config"));
    }
    else
    {
        UE_LOG(LogHunYuanUI, Warning, TEXT("No valid credentials in config, please enter them manually"));
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
        UE_LOG(LogHunYuanUI, Log, TEXT("API credentials updated from UI"));
        AddSystemMessage(TEXT("凭证已生效"));
    }
    else
    {
        AddSystemMessage(TEXT("配置已保存，但凭证不完整"));
    }
    return FReply::Handled();
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

// ==================== 生成任务 ====================

FReply SAIChatWindow::OnSendButtonClicked()
{
    FString Prompt = InputTextBox->GetText().ToString().TrimStartAndEnd();

    if (Prompt.IsEmpty() && CurrentImagePath.IsEmpty())
    {
        AddErrorMessage(TEXT("请输入文字描述或上传图片"));
        return FReply::Handled();
    }

    // 检查API是否就绪
    auto Config = FHunYuanConfigManager::Get()->GetConfig();
    if (!Config.HasValidCredentials())
    {
        AddErrorMessage(TEXT("请先配置API凭证"));
        return FReply::Handled();
    }

    // 添加用户消息
    FString UserMessage = Prompt;
    if (!CurrentImagePath.IsEmpty())
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

    // 提交任务
    if (!CurrentImagePath.IsEmpty())
    {
        SubmitImageTo3D(CurrentImagePath);
    }
    else
    {
        SubmitTextTo3D(Prompt);
    }

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

    FHunYuanAPI::Get()->SubmitProJobFromText(Prompt,
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

    // TODO: 实现图生3D API调用
    AddErrorMessage(TEXT("图生3D功能开发中"));
    ClearProgress();
}

void SAIChatWindow::OnJobSubmitted(bool bSuccess, const FString& JobIdOrError)
{
    if (bSuccess)
    {
        CurrentTask.JobId = JobIdOrError;
        CurrentTask.Status = Chat::EGenerationStatus::Waiting;

        AddSystemMessage(FString::Printf(TEXT("任务提交成功！JobId: %s"), *JobIdOrError));
        UpdateProgress(CurrentTask);

        // 开始轮询
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
    FHunYuanAPI::Get()->QueryProJobResult(JobId,
        FOnJobQueried::CreateSP(this, &SAIChatWindow::OnJobQueried));
}

void SAIChatWindow::OnJobQueried(bool bSuccess, const TSharedPtr<FJsonObject>& Result)
{
    if (!bSuccess || !Result.IsValid())
    {
        // 继续轮询
        if (CurrentTask.Status == Chat::EGenerationStatus::Waiting)
        {
            TWeakPtr<SAIChatWindow> WeakThisPtr = SharedThis(this);  // 先创建 WeakPtr
            FString JobId = CurrentTask.JobId;

            FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateLambda([WeakThisPtr, JobId = CurrentTask.JobId](float) -> bool
                    {
                        auto SharedThis = WeakThisPtr.Pin();
                        if (SharedThis.IsValid())
                        {
                            SharedThis->PollJobResult(JobId);
                        }
                        return false;
                    }),
                3.0f
            );
        }
        return;
    }

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

        FString ModelUrl = JobResult.GetFirstModelUrl();
        if (!ModelUrl.IsEmpty())
        {
            AddSystemMessage(TEXT("模型生成成功，开始下载..."));

            auto Config = FHunYuanConfigManager::Get()->GetConfig();

            Downloader->AddDownload(
                ModelUrl,
                CurrentTask.JobId,
                Config.DownloadDirectory,
                FOnDownloadItemComplete::CreateSP(this, &SAIChatWindow::OnDownloadItemComplete)
            );
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
        TWeakPtr<SAIChatWindow> WeakThisPtr = SharedThis(this);  // 先创建 WeakPtr
        FString JobId = CurrentTask.JobId;

        // 继续轮询
        FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([WeakThisPtr, JobId](float) -> bool
                {
                    auto SharedThis = WeakThisPtr.Pin();
                    if (SharedThis.IsValid())
                    {
                        SharedThis->PollJobResult(JobId);
                    }
                    return false;
                }),
            3.0f
        );
    }
}

// ==================== 下载处理 ====================

void SAIChatWindow::OnDownloadProgress(const Download::FDownloadItem& Item)
{
    if (Item.JobId == CurrentTask.JobId)
    {
        CurrentTask.Status = Chat::EGenerationStatus::Downloading;
        CurrentTask.Progress = Item.Progress;
        CurrentTask.StatusMessage = FString::Printf(TEXT("下载中 %.1f%% (%s/%s)"),
            Item.Progress * 100.0f,
            *Item.GetReceivedSizeString(),
            *Item.GetTotalSizeString());

        UpdateProgress(CurrentTask);
    }
}

void SAIChatWindow::OnDownloadComplete(bool bSuccess, const FString& FilePath)
{
    // 这个回调会在下载完成时触发，但我们使用 OnDownloadItemComplete 来处理具体任务
}

void SAIChatWindow::OnDownloadItemComplete(const Download::FDownloadItem& Item)
{
    if (Item.JobId == CurrentTask.JobId)
    {
        if (Item.Status == Download::EDownloadStatus::Completed)
        {
            CurrentTask.Status = Chat::EGenerationStatus::Extracting;
            UpdateProgress(CurrentTask);

            HandleDownloadedFile(Item.DestinationPath, Item.JobId);
        }
        else
        {
            CurrentTask.Status = Chat::EGenerationStatus::Failed;
            AddErrorMessage(TEXT("下载失败: ") + Item.ErrorMessage);
            ClearProgress();
        }
    }
}

void SAIChatWindow::HandleDownloadedFile(const FString& FilePath, const FString& JobId)
{
    FString FinalModelPath = FilePath;
    bool bIsZipFile = FZipExtractor::IsZipFile(FilePath);

    // 如果是ZIP文件，自动解压
    if (bIsZipFile)
    {
        AddSystemMessage(TEXT("检测到ZIP文件，正在自动解压..."));

        FString ModelDir;
        if (FZipExtractor::ExtractModelFromZip(FilePath, ModelDir))
        {
            FinalModelPath = ModelDir;
            AddSystemMessage(FString::Printf(TEXT("解压成功，找到模型: %s"),
                *FPaths::GetCleanFilename(FinalModelPath)));
        }
        else
        {
            AddErrorMessage(TEXT("解压失败，请检查ZIP文件"));
            ClearProgress();
            return;
        }
    }

    // 创建模型信息
    FModelInfo NewInfo;
    NewInfo.FilePath = FinalModelPath;
    NewInfo.FileName = FPaths::GetCleanFilename(FinalModelPath);
    NewInfo.JobId = JobId;
    NewInfo.URL = CurrentTask.ImagePath.IsEmpty() ? CurrentTask.Prompt : CurrentTask.ImagePath;
    NewInfo.DownloadTime = FDateTime::Now();

    // 获取目录大小（所有文件总和）
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    int64 TotalSize = 0;
    TArray<FString> AllFiles;
    PlatformFile.FindFilesRecursively(AllFiles, *FinalModelPath, TEXT("*"));
    for (const FString& File : AllFiles)
    {
        TotalSize += PlatformFile.FileSize(*File);
    }
    NewInfo.FileSize = TotalSize;

    // 添加到历史
    AddToHistory(NewInfo);

    // 任务完成
    CurrentTask.Status = Chat::EGenerationStatus::Completed;
    CurrentTask.EndTime = FDateTime::Now();

    AddSystemMessage(FString::Printf(TEXT("生成完成！耗时: %.1f秒"), CurrentTask.GetElapsedTime()));

    // 清除进度显示
    ClearProgress();

    // 显示预览窗口
    auto Config = FHunYuanConfigManager::Get()->GetConfig();
    if (Config.bShowPreviewAfterDownload)
    {
        ShowModelPreview(NewInfo);
    }

    // 自动导入
    if (Config.bAutoImport)
    {
        ImportModelFromFolder(NewInfo);
    }
}

// ==================== 模型处理 ====================

void SAIChatWindow::ShowModelPreview(const FModelInfo& ModelInfo)
{
    UE_LOG(LogHunYuanAI, Log, TEXT("ShowModelPreview called for: %s"), *ModelInfo.FileName);

    // 确保在游戏线程
    if (!IsInGameThread())
    {
        UE_LOG(LogHunYuanAI, Warning, TEXT("ShowModelPreview called from non-game thread, dispatching to game thread"));
        AsyncTask(ENamedThreads::GameThread, [this, ModelInfo]()
            {
                ShowModelPreview(ModelInfo);
            });
        return;
    }

    // 如果已经有窗口，先关闭
    if (ModelInfoWindow.IsValid())
    {
        UE_LOG(LogHunYuanAI, Log, TEXT("Closing existing preview window"));
        ModelInfoWindow->RequestDestroyWindow();
        ModelInfoWindow.Reset();
    }

    // 检查 Slate 应用程序是否初始化
    if (!FSlateApplication::IsInitialized())
    {
        UE_LOG(LogHunYuanAI, Error, TEXT("FSlateApplication is not initialized"));
        return;
    }

    UE_LOG(LogHunYuanAI, Log, TEXT("Creating new preview window"));

    // 创建新的窗口
    ModelInfoWindow = SNew(SWindow)
        .Title(FText::Format(LOCTEXT("ModelPreview", "模型预览 - {0}"),
            FText::FromString(ModelInfo.FileName)))
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
        UE_LOG(LogHunYuanAI, Error, TEXT("Failed to create SWindow"));
        return;
    }

    // 创建模型信息控件
    TSharedRef<SModelInfoWidget> ModelInfoWidget = SNew(SModelInfoWidget)
        .ModelInfo(ModelInfo)
        .OnImportClicked(FOnModelAction::CreateSP(this, &SAIChatWindow::ImportModelFromFolder))
        .OnPreviewClicked(FOnModelAction::CreateSP(this, &SAIChatWindow::PreviewModel))
        .OnDeleteClicked(FOnModelAction::CreateSP(this, &SAIChatWindow::DeleteModelFile))
        .OnOpenFolderClicked(FOnModelAction::CreateSP(this, &SAIChatWindow::OpenModelFolder));

    // 设置窗口内容
    ModelInfoWindow->SetContent(ModelInfoWidget);

    // 将窗口添加到 Slate 应用程序
    FSlateApplication::Get().AddWindow(ModelInfoWindow.ToSharedRef());

    UE_LOG(LogHunYuanAI, Log, TEXT("Preview window created successfully"));
}

// 导入模型
void SAIChatWindow::ImportModelFromFolder(const FModelInfo& ModelInfo)
{
    UE_LOG(LogTemp, Log, TEXT("ImportModelFromFolder called for: %s"), *ModelInfo.FileName);
    UE_LOG(LogTemp, Log, TEXT("Original File Path: %s"), *ModelInfo.FilePath);

    // 检查原始文件是否存在（压缩包）
    if (!ModelInfo.IsFileExists())
    {
        UE_LOG(LogTemp, Error, TEXT("Original file does not exist: %s"), *ModelInfo.FilePath);

        FNotificationInfo Info(FText::Format(
            NSLOCTEXT("AIChat", "FileNotFound", "文件不存在: {0}"),
            FText::FromString(ModelInfo.FileName)
        ));
        Info.ExpireDuration = 3.0f;
        Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Error"));
        FSlateNotificationManager::Get().AddNotification(Info);
        return;
    }

    // 获取解压后的文件夹路径
    FString FolderPath = FPaths::GetPath(ModelInfo.FilePath);  // 获取 Downloads 目录
    FString BaseFilename = FPaths::GetBaseFilename(ModelInfo.FilePath);  // 获取不带扩展名的文件名
    FString ExtractedFolderPath = FolderPath / BaseFilename;  // 组合成解压文件夹路径

    UE_LOG(LogTemp, Log, TEXT("Looking for extracted folder: %s"), *ExtractedFolderPath);

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    // 检查解压后的文件夹是否存在
    if (!PlatformFile.DirectoryExists(*ExtractedFolderPath))
    {
        UE_LOG(LogTemp, Error, TEXT("Extracted folder does not exist: %s"), *ExtractedFolderPath);

        FNotificationInfo Info(FText::Format(
            NSLOCTEXT("AIChat", "FolderNotFound", "解压文件夹不存在: {0}"),
            FText::FromString(BaseFilename)
        ));
        Info.ExpireDuration = 3.0f;
        Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Error"));
        FSlateNotificationManager::Get().AddNotification(Info);
        return;
    }

    // 在解压文件夹中查找 OBJ 文件
    TArray<FString> FoundFiles;
    PlatformFile.FindFiles(FoundFiles, *ExtractedFolderPath, TEXT("*.obj"));

    if (FoundFiles.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("No OBJ file found in extracted folder: %s"), *ExtractedFolderPath);

        FNotificationInfo Info(NSLOCTEXT("AIChat", "NoObjFile", "解压文件夹中未找到OBJ文件"));
        Info.ExpireDuration = 3.0f;
        Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Error"));
        FSlateNotificationManager::Get().AddNotification(Info);
        return;
    }

    // 使用找到的第一个 OBJ 文件
    FString ObjFilePath = FoundFiles[0];
    FString ObjFileName = FPaths::GetCleanFilename(ObjFilePath);

    UE_LOG(LogTemp, Log, TEXT("Found OBJ file: %s"), *ObjFileName);
    UE_LOG(LogTemp, Log, TEXT("Full OBJ path: %s"), *ObjFilePath);
    UE_LOG(LogTemp, Log, TEXT("Importing folder: %s"), *ExtractedFolderPath);

    // 调用导入管理器
    auto LocalImportManager = FModelImportManager::Get();
    if (LocalImportManager.IsValid())
    {
        // 绑定导入完成事件
        LocalImportManager->OnModelImported.AddSP(this, &SAIChatWindow::HandleModelImported);

        // 使用解压文件夹名作为目标子文件夹名
        FString DestinationPath = TEXT("/Game/HunyuanImports/") + BaseFilename + TEXT("/");

        UE_LOG(LogTemp, Log, TEXT("Destination Path: %s"), *DestinationPath);

        // 传入解压文件夹路径
        bool bSuccess = LocalImportManager->ImportModelFromFolder(ExtractedFolderPath, DestinationPath);

        if (bSuccess)
        {
            UE_LOG(LogTemp, Log, TEXT("Import started successfully"));

            FNotificationInfo Info(FText::Format(
                NSLOCTEXT("AIChat", "ImportStarted", "开始导入模型: {0}"),
                FText::FromString(ObjFileName)
            ));
            Info.ExpireDuration = 2.0f;
            FSlateNotificationManager::Get().AddNotification(Info);

            // 可以选择关闭窗口
            if (ModelInfoWindow.IsValid())
            {
                ModelInfoWindow->RequestDestroyWindow();
                ModelInfoWindow.Reset();
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to start import"));

            FNotificationInfo Info(NSLOCTEXT("AIChat", "ImportStartFailed", "导入启动失败"));
            Info.ExpireDuration = 3.0f;
            Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Error"));
            FSlateNotificationManager::Get().AddNotification(Info);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("ImportManager is not valid"));

        FNotificationInfo Info(NSLOCTEXT("AIChat", "ImportManagerError", "导入管理器初始化失败"));
        Info.ExpireDuration = 3.0f;
        Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Error"));
        FSlateNotificationManager::Get().AddNotification(Info);
    }
}

// 预览模型
void SAIChatWindow::PreviewModel(const FModelInfo& ModelInfo)
{
    UE_LOG(LogTemp, Log, TEXT("PreviewModel called for: %s"), *ModelInfo.FileName);
    
    // TODO: 实现模型预览逻辑
    // 可以打开一个3D预览窗口
    
    FNotificationInfo Info(FText::Format(
        NSLOCTEXT("AIChat", "PreviewNotImplemented", "预览功能开发中: {0}"),
        FText::FromString(ModelInfo.FileName)
    ));
    Info.ExpireDuration = 2.0f;
    FSlateNotificationManager::Get().AddNotification(Info);
}

// 删除模型文件
void SAIChatWindow::DeleteModelFile(const FModelInfo& ModelInfo)
{
    UE_LOG(LogTemp, Log, TEXT("DeleteModelFile called for: %s"), *ModelInfo.FileName);
    UE_LOG(LogTemp, Log, TEXT("File Path: %s"), *ModelInfo.FilePath);

    // 确认对话框
    FText DialogText = FText::Format(
        NSLOCTEXT("AIChat", "ConfirmDelete", "确定要删除模型 \"{0}\" 吗？\n\n文件: {1}\n大小: {2}\n\n此操作不可撤销！"),
        FText::FromString(ModelInfo.FileName),
        FText::FromString(ModelInfo.FilePath),
        FText::FromString(ModelInfo.GetFileSizeString())
    );

    EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, DialogText);
    
    if (Result == EAppReturnType::Yes)
    {
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        bool bDeleted = false;
        
        // 删除主文件
        if (PlatformFile.FileExists(*ModelInfo.FilePath))
        {
            bDeleted = PlatformFile.DeleteFile(*ModelInfo.FilePath);
            UE_LOG(LogTemp, Log, TEXT("Deleted file: %s"), *ModelInfo.FilePath);
        }
        
        // 删除同名的 MTL 和纹理文件
        if (bDeleted)
        {
            FString FolderPath = FPaths::GetPath(ModelInfo.FilePath);
            FString BaseFilename = FPaths::GetBaseFilename(ModelInfo.FilePath);
            
            // 删除 MTL 文件
            FString MtlPath = FolderPath / BaseFilename + TEXT(".mtl");
            if (PlatformFile.FileExists(*MtlPath))
            {
                PlatformFile.DeleteFile(*MtlPath);
                UE_LOG(LogTemp, Log, TEXT("Deleted MTL file: %s"), *MtlPath);
            }
            
            // 删除纹理文件
            TArray<FString> TextureExtensions = { TEXT(".png"), TEXT(".jpg"), TEXT(".jpeg"), TEXT(".tga"), TEXT(".bmp"), TEXT(".dds") };
            for (const FString& Ext : TextureExtensions)
            {
                FString TexturePath = FolderPath / BaseFilename + Ext;
                if (PlatformFile.FileExists(*TexturePath))
                {
                    PlatformFile.DeleteFile(*TexturePath);
                    UE_LOG(LogTemp, Log, TEXT("Deleted texture file: %s"), *TexturePath);
                    break;
                }
            }
        }
        
        if (bDeleted)
        {
            FNotificationInfo Info(FText::Format(
                NSLOCTEXT("AIChat", "DeleteSuccess", "已删除: {0}"),
                FText::FromString(ModelInfo.FileName)
            ));
            Info.ExpireDuration = 3.0f;
            FSlateNotificationManager::Get().AddNotification(Info);
            
            // 刷新模型列表
            RefreshModelList();
            
            // 关闭窗口
            if (ModelInfoWindow.IsValid())
            {
                ModelInfoWindow->RequestDestroyWindow();
                ModelInfoWindow.Reset();
            }
        }
        else
        {
            FNotificationInfo Info(FText::Format(
                NSLOCTEXT("AIChat", "DeleteFailed", "删除失败: {0}"),
                FText::FromString(ModelInfo.FileName)
            ));
            Info.ExpireDuration = 3.0f;
            Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Error"));
            FSlateNotificationManager::Get().AddNotification(Info);
        }
    }
}

// 打开模型所在文件夹
void SAIChatWindow::OpenModelFolder(const FModelInfo& ModelInfo)
{
    UE_LOG(LogTemp, Log, TEXT("OpenModelFolder called for: %s"), *ModelInfo.FileName);
    UE_LOG(LogTemp, Log, TEXT("Original File Path: %s"), *ModelInfo.FilePath);

    // 获取解压后的文件夹路径
    // 假设压缩包路径如：.../Downloads/xxx.zip
    // 解压后文件夹应该是：.../Downloads/xxx（去掉.zip）
    FString FolderPath = FPaths::GetPath(ModelInfo.FilePath);  // 获取 Downloads 目录
    FString BaseFilename = FPaths::GetBaseFilename(ModelInfo.FilePath);  // 获取不带扩展名的文件名
    FString ExtractedFolderPath = FolderPath / BaseFilename;  // 组合成解压文件夹路径

    UE_LOG(LogTemp, Log, TEXT("Looking for extracted folder: %s"), *ExtractedFolderPath);

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    // 先检查解压后的文件夹是否存在
    if (PlatformFile.DirectoryExists(*ExtractedFolderPath))
    {
        UE_LOG(LogTemp, Log, TEXT("Found extracted folder, opening: %s"), *ExtractedFolderPath);
        FPlatformProcess::ExploreFolder(*ExtractedFolderPath);
        return;
    }

    // 如果解压文件夹不存在，回退到原始文件所在目录
    FString DirectoryPath = FPaths::GetPath(ModelInfo.FilePath);
    if (PlatformFile.DirectoryExists(*DirectoryPath))
    {
        UE_LOG(LogTemp, Log, TEXT("Extracted folder not found, opening parent directory: %s"), *DirectoryPath);
        FPlatformProcess::ExploreFolder(*DirectoryPath);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Neither extracted folder nor parent directory exists"));

        FNotificationInfo Info(FText::Format(
            NSLOCTEXT("AIChat", "FolderNotFound", "文件夹不存在: {0}"),
            FText::FromString(ModelInfo.FileName)
        ));
        Info.ExpireDuration = 3.0f;
        Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Error"));
        FSlateNotificationManager::Get().AddNotification(Info);
    }
}

// 处理导入结果
void SAIChatWindow::HandleModelImported(bool bSuccess, const FString& AssetPath)
{
    if (bSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("Model imported successfully: %s"), *AssetPath);

        FNotificationInfo Info(FText::Format(
            NSLOCTEXT("AIChat", "ImportSuccess", "模型导入成功: {0}"),
            FText::FromString(FPaths::GetBaseFilename(AssetPath))
        ));
        Info.ExpireDuration = 3.0f;
        Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Note"));
        FSlateNotificationManager::Get().AddNotification(Info);

        // 刷新模型列表
        RefreshModelList();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Model import failed"));

        FNotificationInfo Info(NSLOCTEXT("AIChat", "ImportFailed", "模型导入失败"));
        Info.ExpireDuration = 3.0f;
        Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Error"));
        FSlateNotificationManager::Get().AddNotification(Info);
    }
}

// 刷新模型列表
void SAIChatWindow::RefreshModelList()
{
    UE_LOG(LogTemp, Log, TEXT("Refreshing model list"));

    // TODO: 根据您的UI实现刷新逻辑
    // 例如，如果您有一个列表视图：
    /*
    if (ModelListView.IsValid())
    {
        ModelListView->RebuildList();
        ModelListView->RequestListRefresh();
    }
    */

    // 或者重新扫描文件夹并更新数据源
    /*
    TArray<FModelInfo> UpdatedModels = ScanForModelFiles();
    UpdateModelList(UpdatedModels);
    */
}

// ==================== 历史管理 ====================

void SAIChatWindow::LoadDownloadHistory()
{
    const FString HistoryFile = FPaths::ProjectSavedDir() / TEXT("HunYuanAI/DownloadHistory.json");

    if (!FPaths::FileExists(HistoryFile))
    {
        return;
    }

    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *HistoryFile))
    {
        UE_LOG(LogHunYuanUI, Error, TEXT("Failed to load download history"));
        return;
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogHunYuanUI, Error, TEXT("Failed to parse download history"));
        return;
    }

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

                FString TimeString = ItemObject->GetStringField(TEXT("DownloadTime"));
                FDateTime::Parse(TimeString, Info.DownloadTime);

                // 检查文件是否仍然存在
                if (FPaths::FileExists(Info.FilePath))
                {
                    DownloadHistory.Add(Info);
                }
            }
        }
    }

    UE_LOG(LogHunYuanUI, Log, TEXT("Loaded %d download history records"), DownloadHistory.Num());
}

void SAIChatWindow::SaveDownloadHistory()
{
    const FString HistoryFile = FPaths::ProjectSavedDir() / TEXT("HunYuanAI/DownloadHistory.json");

    // 确保目录存在
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
        UE_LOG(LogHunYuanUI, Log, TEXT("Saved %d download history records"), DownloadHistory.Num());
    }
}

void SAIChatWindow::AddToHistory(const FModelInfo& ModelInfo)
{
    DownloadHistory.Add(ModelInfo);
    SaveDownloadHistory();
}

// ==================== UI 状态更新 ====================

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
    if (CurrentTask.Status == Chat::EGenerationStatus::Waiting)
    {
        // 等待状态显示不确定进度
        return TOptional<float>();
    }
    return CurrentTask.Progress;
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
            OutFolder))
        {
            if (!OutFolder.IsEmpty())
            {
                auto Config = FHunYuanConfigManager::Get()->GetConfig();
                Config.DownloadDirectory = OutFolder;
                FHunYuanConfigManager::Get()->UpdateConfig(Config);
            }
        }
    }
    return FReply::Handled();
}

// ==================== 配置管理辅助函数 ====================

void SAIChatWindow::OnRememberPasswordCheckStateChanged(ECheckBoxState NewState)
{
    bool bRemember = (NewState == ECheckBoxState::Checked);

    // 获取当前配置
    auto ConfigManager = FHunYuanConfigManager::Get();
    auto Config = ConfigManager->GetConfig();

    // 更新记住密码状态
    Config.bRememberPassword = bRemember;

    // 如果不记住密码，清空凭证
    if (!bRemember)
    {
        Config.SecretId.Empty();
        Config.SecretKey.Empty();

        // 更新UI
        SecretIdInput->SetText(FText::GetEmpty());
        SecretKeyInput->SetText(FText::GetEmpty());
    }

    // 保存配置
    ConfigManager->UpdateConfig(Config);

    AddSystemMessage(bRemember ? TEXT("已启用记住密码") : TEXT("已禁用记住密码"));
}

void SAIChatWindow::UpdateCredentialsUI()
{
    auto Config = FHunYuanConfigManager::Get()->GetConfig();

    // 根据记住密码状态更新UI提示
    if (!Config.bRememberPassword && Config.HasValidCredentials())
    {
        // 不记住密码但有保存的凭证，显示提示文本
        SecretIdInput->SetHintText(FText::Format(
            LOCTEXT("SecretIdHintWithSaved", "SecretId已保存 (勾选“记住密码”后显示) - {0}..."),
            FText::FromString(Config.SecretId.Left(8))
        ));

        SecretKeyInput->SetHintText(LOCTEXT("SecretKeyHintWithSaved", "SecretKey已保存 (勾选“记住密码”后显示)"));

        // 清空输入框
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

#undef LOCTEXT_NAMESPACE