#include "UI/ModelInfoWidget.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SBox.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Brushes/SlateImageBrush.h"
#include "HunYuanAI.h"

#define LOCTEXT_NAMESPACE "SModelInfoWidget"

void SModelInfoWidget::Construct(const FArguments& InArgs)
{
    ModelInfo = InArgs._ModelInfo;
    OnImportDelegate = InArgs._OnImportClicked;
    OnPreviewDelegate = InArgs._OnPreviewClicked;
    OnDeleteDelegate = InArgs._OnDeleteClicked;
    OnOpenFolderDelegate = InArgs._OnOpenFolderClicked;
    CurrentPreviewType = EModelPreviewType::BasicInfo;

    // 如果文件大小为0，尝试获取
    if (ModelInfo.FileSize == 0 && !ModelInfo.FilePath.IsEmpty())
    {
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        ModelInfo.FileSize = PlatformFile.FileSize(*ModelInfo.FilePath);
    }

    // 预览类型选项
    PreviewTypeOptions.Add(MakeShareable(new EModelPreviewType(EModelPreviewType::BasicInfo)));
    PreviewTypeOptions.Add(MakeShareable(new EModelPreviewType(EModelPreviewType::Thumbnail)));
    PreviewTypeOptions.Add(MakeShareable(new EModelPreviewType(EModelPreviewType::Wireframe)));

    TSharedPtr<EModelPreviewType> InitiallySelected = nullptr;
    for (auto& Option : PreviewTypeOptions)
    {
        if (*Option == CurrentPreviewType)
        {
            InitiallySelected = Option;
            break;
        }
    }

    ChildSlot
        [
            SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                .BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f))
                .Padding(15)
                [
                    SNew(SVerticalBox)

                        // 标题
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 0, 0, 15)
                        [
                            SNew(STextBlock)
                                .Text(FText::Format(LOCTEXT("ModelInfoTitle", "模型信息 - {0}"),
                                    FText::FromString(ModelInfo.FileName)))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
                        ]

                        // 预览类型选择
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 0, 0, 10)
                        [
                            SNew(SHorizontalBox)
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                .Padding(0, 0, 10, 0)
                                [
                                    SNew(STextBlock)
                                        .Text(LOCTEXT("PreviewType", "预览类型:"))
                                ]
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                [
                                    SNew(SComboBox<TSharedPtr<EModelPreviewType>>)
                                        .OptionsSource(&PreviewTypeOptions)
                                        .InitiallySelectedItem(InitiallySelected)
                                        .OnSelectionChanged(this, &SModelInfoWidget::OnPreviewTypeChanged)
                                        .OnGenerateWidget(this, &SModelInfoWidget::GeneratePreviewTypeWidget)
                                        [
                                            SNew(STextBlock)
                                                .Text_Lambda([this]() {
                                                return GetPreviewTypeText(CurrentPreviewType);
                                                    })
                                        ]
                                ]
                        ]

                    // 预览区域
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 0, 0, 15)
                        [
                            SNew(SBox)
                                .WidthOverride(300)
                                .HeightOverride(200)
                                .Visibility_Lambda([this]() {
                                return CurrentPreviewType != EModelPreviewType::BasicInfo ?
                                    EVisibility::Visible : EVisibility::Collapsed;
                                    })
                                [
                                    SNew(SBorder)
                                        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                                        .BorderBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f, 1.0f))
                                        .HAlign(HAlign_Center)
                                        .VAlign(VAlign_Center)
                                        [
                                            SAssignNew(PreviewImage, SImage)
                                                .Image(FCoreStyle::Get().GetDefaultBrush())
                                        ]
                                ]
                        ]

                    // 信息网格
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 0, 0, 15)
                        [
                            SNew(SGridPanel)
                                .FillColumn(1, 1.0f)

                                // 文件名
                                + SGridPanel::Slot(0, 0)
                                .Padding(5)
                                [
                                    SNew(STextBlock)
                                        .Text(LOCTEXT("FileName", "文件名:"))
                                        .ColorAndOpacity(FLinearColor::Gray)
                                ]
                                + SGridPanel::Slot(1, 0)
                                .Padding(5)
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(ModelInfo.FileName))
                                        .ToolTipText(FText::FromString(ModelInfo.FilePath))
                                ]

                                // 格式
                                + SGridPanel::Slot(0, 1)
                                .Padding(5)
                                [
                                    SNew(STextBlock)
                                        .Text(LOCTEXT("Format", "格式:"))
                                        .ColorAndOpacity(FLinearColor::Gray)
                                ]
                                + SGridPanel::Slot(1, 1)
                                .Padding(5)
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(ModelInfo.Format.IsEmpty() ?
                                            FPaths::GetExtension(ModelInfo.FileName) : ModelInfo.Format))
                                ]

                                // 大小
                                + SGridPanel::Slot(0, 2)
                                .Padding(5)
                                [
                                    SNew(STextBlock)
                                        .Text(LOCTEXT("FileSize", "大小:"))
                                        .ColorAndOpacity(FLinearColor::Gray)
                                ]
                                + SGridPanel::Slot(1, 2)
                                .Padding(5)
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(ModelInfo.GetFileSizeString()))
                                ]

                                // 下载时间
                                + SGridPanel::Slot(0, 3)
                                .Padding(5)
                                [
                                    SNew(STextBlock)
                                        .Text(LOCTEXT("DownloadTime", "下载时间:"))
                                        .ColorAndOpacity(FLinearColor::Gray)
                                ]
                                + SGridPanel::Slot(1, 3)
                                .Padding(5)
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(ModelInfo.DownloadTime.ToString(TEXT("%Y-%m-%d %H:%M:%S"))))
                                ]

                                // Job ID
                                + SGridPanel::Slot(0, 4)
                                .Padding(5)
                                [
                                    SNew(STextBlock)
                                        .Text(LOCTEXT("JobId", "任务ID:"))
                                        .ColorAndOpacity(FLinearColor::Gray)
                                ]
                                + SGridPanel::Slot(1, 4)
                                .Padding(5)
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(ModelInfo.JobId))
                                ]
                        ]

                    // 操作按钮
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        .HAlign(HAlign_Center)
                        .Padding(0, 10, 0, 0)
                        [
                            SNew(SHorizontalBox)

                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(5)
                                [
                                    SNew(SButton)
                                        .Text(LOCTEXT("Preview", "预览模型"))
                                        .OnClicked(this, &SModelInfoWidget::OnPreviewButtonClicked)
                                ]

                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(5)
                                [
                                    SNew(SButton)
                                        .Text(LOCTEXT("Import", "导入到UE"))
                                        .ButtonColorAndOpacity(FLinearColor(0.2f, 0.5f, 0.2f, 1.0f))
                                        .OnClicked(this, &SModelInfoWidget::OnImportButtonClicked)
                                ]

                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(5)
                                [
                                    SNew(SButton)
                                        .Text(LOCTEXT("OpenFolder", "打开文件夹"))
                                        .OnClicked(this, &SModelInfoWidget::OnOpenFolderButtonClicked)
                                ]

                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(5)
                                [
                                    SNew(SButton)
                                        .Text(LOCTEXT("Delete", "删除文件"))
                                        .ButtonColorAndOpacity(FLinearColor(0.5f, 0.2f, 0.2f, 1.0f))
                                        .OnClicked(this, &SModelInfoWidget::OnDeleteButtonClicked)
                                ]
                        ]

                    // URL信息
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 15, 0, 0)
                        [
                            SNew(SScrollBox)
                                + SScrollBox::Slot()
                                [
                                    SNew(STextBlock)
                                        .Text(FText::Format(LOCTEXT("SourceURL", "源URL: {0}"),
                                            FText::FromString(ModelInfo.URL)))
                                        .AutoWrapText(true)
                                        .ColorAndOpacity(FLinearColor::Gray)
                                ]
                        ]
                ]
        ];
}

TSharedRef<SWidget> SModelInfoWidget::CreateInfoRow(const FString& Label, const FString& Value, const FLinearColor& ValueColor)
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(0, 0, 10, 0)
        [
            SNew(STextBlock)
                .Text(FText::FromString(Label + TEXT(":")))
                .ColorAndOpacity(FLinearColor::Gray)
        ]
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        [
            SNew(STextBlock)
                .Text(FText::FromString(Value))
                .ColorAndOpacity(ValueColor)
        ];
}

const FSlateBrush* SModelInfoWidget::GetFileIcon() const
{
    return FCoreStyle::Get().GetDefaultBrush();
}

TSharedRef<SWidget> SModelInfoWidget::GeneratePreviewTypeWidget(TSharedPtr<EModelPreviewType> InType)
{
    return SNew(STextBlock)
        .Text(GetPreviewTypeText(*InType));
}

FText SModelInfoWidget::GetPreviewTypeText(EModelPreviewType Type) const
{
    switch (Type)
    {
    case EModelPreviewType::BasicInfo:
        return LOCTEXT("BasicInfo", "基本信息");
    case EModelPreviewType::Thumbnail:
        return LOCTEXT("Thumbnail", "缩略图");
    case EModelPreviewType::Wireframe:
        return LOCTEXT("Wireframe", "线框预览");
    default:
        return LOCTEXT("None", "无");
    }
}

void SModelInfoWidget::OnPreviewTypeChanged(TSharedPtr<EModelPreviewType> NewType, ESelectInfo::Type SelectInfo)
{
    if (NewType.IsValid())
    {
        CurrentPreviewType = *NewType;
    }
}

FReply SModelInfoWidget::OnImportButtonClicked()
{
    OnImportDelegate.ExecuteIfBound(ModelInfo);
    return FReply::Handled();
}

FReply SModelInfoWidget::OnPreviewButtonClicked()
{
    OnPreviewDelegate.ExecuteIfBound(ModelInfo);
    return FReply::Handled();
}

FReply SModelInfoWidget::OnDeleteButtonClicked()
{
    OnDeleteDelegate.ExecuteIfBound(ModelInfo);
    return FReply::Handled();
}

FReply SModelInfoWidget::OnOpenFolderButtonClicked()
{
    OnOpenFolderDelegate.ExecuteIfBound(ModelInfo);
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE