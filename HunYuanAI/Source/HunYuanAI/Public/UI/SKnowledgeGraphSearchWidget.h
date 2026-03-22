// SKnowledgeGraphSearchWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "UERagSubsystem.h"
#include "RagTypes.h"

DECLARE_DELEGATE_OneParam(FOnDocumentSelected, const FSourceDocument&);

class SKnowledgeGraphSearchWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SKnowledgeGraphSearchWidget) {}
        SLATE_EVENT(FOnDocumentSelected, OnDocumentSelected)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // 清空搜索结果
    void ClearResults();

private:
    // UI组件
    TSharedPtr<SEditableTextBox> SearchInput;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> FileTypeFilter;
    TSharedPtr<SListView<TSharedPtr<FSourceDocument>>> SearchResultsView;
    TSharedPtr<SButton> SearchButton;
    TSharedPtr<SBox> ResultDetailsPanel;
    TSharedPtr<STextBlock> ResultTitleText;
    TSharedPtr<STextBlock> ResultContentText;

    // 数据
    TArray<TSharedPtr<FSourceDocument>> CurrentResults;
    TArray<TSharedPtr<FString>> FilterOptions;
    FString CurrentFilterType;

    // 回调
    FOnDocumentSelected OnDocumentSelected;

    // 搜索相关
    void OnSearchTextChanged(const FText& Text);
    FReply OnSearchClicked();
    void OnSearchComplete(const FQueryResponse& Response);
    void OnSearchResultClicked(TSharedPtr<FSourceDocument> Item, ESelectInfo::Type SelectInfo);
    void OnDocumentDoubleClicked(TSharedPtr<FSourceDocument> Item);

    // 筛选相关
    void InitFilterOptions();
    void OnFileTypeFilterChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);
    void RefreshFilterOptions();

    // 获取RAG子系统
    UUERagSubsystem* GetRagSubsystem() const;

    // 显示文档详情
    void ShowDocumentDetail(const FSourceDocument& Document);
    void HideDocumentDetail();

    // 辅助函数
    FString GetDocumentIcon(const FString& ResourceType) const;
    FText GetFormattedContent(const FString& Content, int32 MaxLength = 200) const;
};