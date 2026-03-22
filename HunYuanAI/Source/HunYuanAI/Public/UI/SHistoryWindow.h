// SHistoryWindow.h
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SButton.h"
#include "RagTypes.h"
#include "UERagSubsystem.h"

class UUERagSubsystem;

enum class EHistoryItemType : uint8
{
    DownloadedModel,
    ImportedAsset,
    SearchResult,
    KGAsset
};

struct FHistoryItem
{
    FString AssetName;
    FString AssetPath;
    FString FileType;
    EHistoryItemType ItemType;
    FDateTime LastUsed;
    FString Thumbnail;
    bool bIsSelected = false;
};

class SHistoryWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHistoryWindow) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void RefreshHistory();

private:
    TSharedRef<ITableRow> GenerateHistoryRow(TSharedPtr<FHistoryItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
    void ApplyFilter();
    void OnHistoryItemDoubleClicked(TSharedPtr<FHistoryItem> Item);
    void ShowAssetDetails(TSharedPtr<FHistoryItem> Item);
    void OnAssetDetailsQueried(const FQueryResponse& Response, TSharedPtr<FHistoryItem> Item);
    void ShowAssetInfoWindow(TSharedPtr<FHistoryItem> Item, const FSourceDocument& Document);
    void OnFileTypeFilterChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);
    UUERagSubsystem* GetRagSubsystem() const;

    TArray<TSharedPtr<FHistoryItem>> AllHistoryItems;
    TSharedPtr<SListView<TSharedPtr<FHistoryItem>>> HistoryListView;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> FileTypeFilter;
    TSharedPtr<SEditableTextBox> SearchInput;
    FString CurrentFilterType;
};