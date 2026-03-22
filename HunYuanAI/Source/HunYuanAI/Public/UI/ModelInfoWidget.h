#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SComboBox.h"

// 模型信息结构
struct FModelInfo
{
    FString FilePath;
    FString FileName;
    FString JobId;
    FString URL;
    FString Format;
    int64 FileSize = 0;
    FDateTime DownloadTime;

    // 获取文件大小字符串
    FString GetFileSizeString() const
    {
        if (FileSize < 1024)
            return FString::Printf(TEXT("%lld B"), FileSize);
        else if (FileSize < 1024 * 1024)
            return FString::Printf(TEXT("%.1f KB"), FileSize / 1024.0f);
        else
            return FString::Printf(TEXT("%.1f MB"), FileSize / (1024.0f * 1024.0f));
    }

    // 检查文件是否存在
    bool IsFileExists() const
    {
        if (FilePath.IsEmpty()) return false;

        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

        // 检查是文件还是文件夹
        if (PlatformFile.FileExists(*FilePath))
        {
            return true;  // 是文件
        }
        else if (PlatformFile.DirectoryExists(*FilePath))
        {
            return true;  // 是文件夹（解压后的目录）
        }

        return false;
    }
};

// 预览选项
enum class EModelPreviewType
{
    BasicInfo,
    Thumbnail,
    Wireframe
};

// 模型操作委托
DECLARE_DELEGATE_OneParam(FOnModelAction, const FModelInfo&);

class HUNYUANAI_API SModelInfoWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SModelInfoWidget) {}
        SLATE_ARGUMENT(FModelInfo, ModelInfo)
        SLATE_EVENT(FOnModelAction, OnImportClicked)
        SLATE_EVENT(FOnModelAction, OnPreviewClicked)
        SLATE_EVENT(FOnModelAction, OnDeleteClicked)
        SLATE_EVENT(FOnModelAction, OnOpenFolderClicked)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    TSharedRef<SWidget> CreateInfoRow(const FString& Label, const FString& Value, const FLinearColor& ValueColor = FLinearColor::White);
    const FSlateBrush* GetFileIcon() const;
    FText GetPreviewTypeText(EModelPreviewType Type) const;

    TSharedRef<SWidget> GeneratePreviewTypeWidget(TSharedPtr<EModelPreviewType> InType);
    void OnPreviewTypeChanged(TSharedPtr<EModelPreviewType> NewType, ESelectInfo::Type SelectInfo);

    FReply OnImportButtonClicked();
    FReply OnPreviewButtonClicked();
    FReply OnDeleteButtonClicked();
    FReply OnOpenFolderButtonClicked();

private:
    FModelInfo ModelInfo;
    FOnModelAction OnImportDelegate;
    FOnModelAction OnPreviewDelegate;
    FOnModelAction OnDeleteDelegate;
    FOnModelAction OnOpenFolderDelegate;

    TSharedPtr<SImage> PreviewImage;
    TSharedPtr<STextBlock> PreviewText;
    TArray<TSharedPtr<EModelPreviewType>> PreviewTypeOptions;
    EModelPreviewType CurrentPreviewType;
};