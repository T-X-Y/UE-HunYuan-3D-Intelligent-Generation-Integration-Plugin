// SAIChatWindowUI.cpp
// UI构建：API配置、导入选项、模型选择、聊天历史、进度、输入等

#include "CoreMinimal.h"
#include "UI/SAIChatWindow.h"

// Slate 核心组件
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Images/SImage.h"

// 样式
#include "Styling/CoreStyle.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "SAIChatWindow"

// ==================== API 配置区域 ====================

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

                // SecretId 输入
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

            // SecretKey 输入
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

// ==================== 导入选项区域 ====================

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

// ==================== 模型选择区域 ====================

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

// ==================== 单图上传区域 ====================

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

// ==================== 聊天历史区域 ====================

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

// ==================== 进度区域 ====================

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

// ==================== 输入区域 ====================

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
                .IsEnabled_Lambda([this]()
                    {
                        return CurrentTask.Status == Chat::EGenerationStatus::Idle;
                    })
        ];
}

#undef LOCTEXT_NAMESPACE