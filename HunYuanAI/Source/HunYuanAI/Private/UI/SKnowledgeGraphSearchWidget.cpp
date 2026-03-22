// SKnowledgeGraphSearchWidget.cpp
#include "UI/SKnowledgeGraphSearchWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Images/SImage.h"
#include "Framework/Application/SlateApplication.h"
#include "EditorStyleSet.h"
#include "RagTypes.h"

#define LOCTEXT_NAMESPACE "KnowledgeGraphSearch"

void SKnowledgeGraphSearchWidget::Construct(const FArguments& InArgs)
{
    OnDocumentSelected = InArgs._OnDocumentSelected;

    // 初始化筛选选项
    InitFilterOptions();

    // 创建搜索结果列表视图
    SearchResultsView = SNew(SListView<TSharedPtr<FSourceDocument>>)
        .ListItemsSource(&CurrentResults)
        .OnGenerateRow_Lambda([this](TSharedPtr<FSourceDocument> Item, const TSharedRef<STableViewBase>& OwnerTable)
            {
                return SNew(STableRow<TSharedPtr<FSourceDocument>>, OwnerTable)
                    .Padding(4)
                    [
                        SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            .Padding(4, 0, 8, 0)
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(Item->ResourceType.IsEmpty() ? TEXT("📄") :
                                        Item->ResourceType == TEXT("uasset") ? TEXT("🎨") : TEXT("📄")))
                                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
                            ]
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .VAlign(VAlign_Center)
                            [
                                SNew(SVerticalBox)
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    [
                                        SNew(STextBlock)
                                            .Text(FText::FromString(Item->ResourceName))
                                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                                    ]
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    [
                                        SNew(STextBlock)
                                            .Text(GetFormattedContent(Item->Content, 100))
                                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                                            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                                    ]
                            ]
                    ];
            })
        .OnSelectionChanged(this, &SKnowledgeGraphSearchWidget::OnSearchResultClicked)
        .OnMouseButtonDoubleClick(this, &SKnowledgeGraphSearchWidget::OnDocumentDoubleClicked);

    // 创建详情面板
    ResultDetailsPanel = SNew(SBox)
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
                            SAssignNew(ResultTitleText, STextBlock)
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
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
                                    SAssignNew(ResultContentText, STextBlock)
                                        .AutoWrapText(true)
                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                ]
                        ]
                ]
        ];

    // 主布局
    ChildSlot
        [
            SNew(SVerticalBox)
                // 搜索栏
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(8, 8, 8, 4)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .Padding(0, 0, 4, 0)
                        [
                            SAssignNew(SearchInput, SEditableTextBox)
                                .HintText(LOCTEXT("SearchHint", "输入关键词搜索知识图谱..."))
                                .OnTextChanged(this, &SKnowledgeGraphSearchWidget::OnSearchTextChanged)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(0, 0, 4, 0)
                        [
                            SAssignNew(FileTypeFilter, SComboBox<TSharedPtr<FString>>)
                                .OptionsSource(&FilterOptions)
                                .OnSelectionChanged(this, &SKnowledgeGraphSearchWidget::OnFileTypeFilterChanged)
                                .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
                                    {
                                        return SNew(STextBlock).Text(FText::FromString(*Item));
                                    })
                                .Content()
                                [
                                    SNew(STextBlock)
                                        .Text_Lambda([this]() -> FText
                                            {
                                                if (CurrentFilterType.IsEmpty())
                                                    return LOCTEXT("AllTypes", "全部类型");
                                                return FText::FromString(CurrentFilterType);
                                            })
                                ]
                        ]
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        [
                            SAssignNew(SearchButton, SButton)
                                .Text(LOCTEXT("Search", "搜索"))
                                .OnClicked(this, &SKnowledgeGraphSearchWidget::OnSearchClicked)
                        ]
                ]
            // 搜索结果
            + SVerticalBox::Slot()
                .FillHeight(0.6f)
                .Padding(8, 0, 8, 4)
                [
                    SearchResultsView.ToSharedRef()
                ]
                // 详情面板
                + SVerticalBox::Slot()
                .FillHeight(0.4f)
                .Padding(8, 0, 8, 8)
                [
                    ResultDetailsPanel.ToSharedRef()
                ]
        ];
}

void SKnowledgeGraphSearchWidget::InitFilterOptions()
{
    FilterOptions.Add(MakeShareable(new FString(TEXT("全部"))));
    FilterOptions.Add(MakeShareable(new FString(TEXT("uasset"))));
    FilterOptions.Add(MakeShareable(new FString(TEXT("mesh"))));
    FilterOptions.Add(MakeShareable(new FString(TEXT("texture"))));
    FilterOptions.Add(MakeShareable(new FString(TEXT("material"))));
    FilterOptions.Add(MakeShareable(new FString(TEXT("blueprint"))));
    CurrentFilterType = TEXT("全部");
}

void SKnowledgeGraphSearchWidget::OnSearchTextChanged(const FText& Text)
{
    // 可以在这里实现实时搜索，但为了性能，我们只在点击搜索按钮时搜索
}

FReply SKnowledgeGraphSearchWidget::OnSearchClicked()
{
    FString Query = SearchInput->GetText().ToString();
    if (Query.IsEmpty())
    {
        return FReply::Handled();
    }

    UUERagSubsystem* RAG = GetRagSubsystem();
    if (!RAG)
    {
        UE_LOG(LogTemp, Warning, TEXT("RAG Subsystem not available"));
        return FReply::Handled();
    }

    // 构建查询请求
    FQueryRequest Request;
    Request.Query = Query;
    Request.TopK = 10;

    // 设置筛选类型
    if (CurrentFilterType != TEXT("全部"))
    {
        Request.FilterType = CurrentFilterType;
    }

    // 显示加载状态
    SearchButton->SetEnabled(false);

    // 创建静态委托，绑定到 OnSearchComplete
    FOnQueryCompleteDelegate Delegate;
    Delegate.BindSP(this, &SKnowledgeGraphSearchWidget::OnSearchComplete);

    // 使用静态委托版本
    RAG->QueryRagAsync(Request, Delegate);

    return FReply::Handled();
}

void SKnowledgeGraphSearchWidget::OnSearchComplete(const FQueryResponse& Response)
{
    SearchButton->SetEnabled(true);

    if (!Response.bSuccess)
    {
        UE_LOG(LogTemp, Warning, TEXT("Search failed: %s"), *Response.ErrorMessage);
        CurrentResults.Empty();
        SearchResultsView->RequestListRefresh();
        HideDocumentDetail();
        return;
    }

    // 转换结果
    CurrentResults.Empty();
    for (const FSourceDocument& Source : Response.Sources)
    {
        CurrentResults.Add(MakeShared<FSourceDocument>(Source));
    }

    SearchResultsView->RequestListRefresh();

    if (CurrentResults.Num() == 0)
    {
        // 显示无结果提示
    }

    UE_LOG(LogTemp, Log, TEXT("Found %d results in %.2f ms"), CurrentResults.Num(), Response.ProcessingTime);
}

void SKnowledgeGraphSearchWidget::OnSearchResultClicked(TSharedPtr<FSourceDocument> Item, ESelectInfo::Type SelectInfo)
{
    if (!Item.IsValid())
    {
        HideDocumentDetail();
        return;
    }

    ShowDocumentDetail(*Item);

    // 通知外部选中了文档
    if (OnDocumentSelected.IsBound())
    {
        OnDocumentSelected.Execute(*Item);
    }
}

void SKnowledgeGraphSearchWidget::OnDocumentDoubleClicked(TSharedPtr<FSourceDocument> Item)
{
    if (!Item.IsValid()) return;

    // 双击时，通知外部选中（可以用于自动填充等）
    if (OnDocumentSelected.IsBound())
    {
        OnDocumentSelected.Execute(*Item);
    }
}

void SKnowledgeGraphSearchWidget::OnFileTypeFilterChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
    if (Selection.IsValid())
    {
        CurrentFilterType = *Selection;
    }
}

void SKnowledgeGraphSearchWidget::RefreshFilterOptions()
{
    // 可以动态刷新筛选选项
}

UUERagSubsystem* SKnowledgeGraphSearchWidget::GetRagSubsystem() const
{
    // 获取GameInstance
    if (GEditor && GEditor->GetEditorWorldContext().World())
    {
        UGameInstance* GameInstance = GEditor->GetEditorWorldContext().World()->GetGameInstance();
        if (GameInstance)
        {
            return GameInstance->GetSubsystem<UUERagSubsystem>();
        }
    }
    return nullptr;
}

void SKnowledgeGraphSearchWidget::ShowDocumentDetail(const FSourceDocument& Document)
{
    ResultDetailsPanel->SetVisibility(EVisibility::Visible);

    FString Title = FString::Printf(TEXT("📄 %s (%s)"),
        *Document.ResourceName,
        *Document.ResourceType);
    ResultTitleText->SetText(FText::FromString(Title));

    FString Content = GetFormattedContent(Document.Content, 500).ToString();
    ResultContentText->SetText(FText::FromString(Content));
}

void SKnowledgeGraphSearchWidget::HideDocumentDetail()
{
    ResultDetailsPanel->SetVisibility(EVisibility::Collapsed);
}

FText SKnowledgeGraphSearchWidget::GetFormattedContent(const FString& Content, int32 MaxLength) const
{
    FString Trimmed = Content;
    if (Trimmed.Len() > MaxLength)
    {
        Trimmed = Trimmed.Left(MaxLength) + TEXT("...");
    }
    return FText::FromString(Trimmed);
}

void SKnowledgeGraphSearchWidget::ClearResults()
{
    CurrentResults.Empty();
    SearchResultsView->RequestListRefresh();
    HideDocumentDetail();
    SearchInput->SetText(FText::GetEmpty());
}

#undef LOCTEXT_NAMESPACE