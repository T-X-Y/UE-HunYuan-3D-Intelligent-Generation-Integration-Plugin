// SHistoryWindow.cpp
#include "UI/SHistoryWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/GameInstance.h"

#define LOCTEXT_NAMESPACE "SHistoryWindow"

void SHistoryWindow::Construct(const FArguments& InArgs)
{
    // 初始化筛选选项
    TArray<TSharedPtr<FString>> FilterOptions;
    FilterOptions.Add(MakeShareable(new FString(TEXT("全部"))));
    FilterOptions.Add(MakeShareable(new FString(TEXT("StaticMesh"))));
    FilterOptions.Add(MakeShareable(new FString(TEXT("Texture2D"))));
    FilterOptions.Add(MakeShareable(new FString(TEXT("Material"))));
    FilterOptions.Add(MakeShareable(new FString(TEXT("Blueprint"))));

    TSharedPtr<FString> InitialSelection = FilterOptions[0];

    ChildSlot
        [
            SNew(SVerticalBox)
                // 搜索栏
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(8)
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .Padding(0, 0, 4, 0)
                        [
                            SAssignNew(SearchInput, SEditableTextBox)
                                .HintText(LOCTEXT("SearchHint", "搜索资产..."))
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(0, 0, 4, 0)
                        [
                            SAssignNew(FileTypeFilter, SComboBox<TSharedPtr<FString>>)
                                .OptionsSource(&FilterOptions)
                                .InitiallySelectedItem(InitialSelection)
                                .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
                                    {
                                        return SNew(STextBlock).Text(FText::FromString(*Item));
                                    })
                                .OnSelectionChanged(this, &SHistoryWindow::OnFileTypeFilterChanged)
                                .Content()
                                [
                                    SNew(STextBlock)
                                        .Text_Lambda([this]() -> FText
                                            {
                                                return CurrentFilterType.IsEmpty() ? LOCTEXT("AllTypes", "全部类型") : FText::FromString(CurrentFilterType);
                                            })
                                ]
                        ]
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        [
                            SNew(SButton)
                                .Text(LOCTEXT("Refresh", "刷新"))
                                .OnClicked_Lambda([this]() { RefreshHistory(); return FReply::Handled(); })
                        ]
                ]
            // 历史列表
            + SVerticalBox::Slot()
                .FillHeight(1.0f)
                .Padding(8)
                [
                    SAssignNew(HistoryListView, SListView<TSharedPtr<FHistoryItem>>)
                        .ListItemsSource(&AllHistoryItems)
                        .OnGenerateRow(this, &SHistoryWindow::GenerateHistoryRow)
                        .OnMouseButtonDoubleClick(this, &SHistoryWindow::OnHistoryItemDoubleClicked)
                ]
        ];

    RefreshHistory();
}

TSharedRef<ITableRow> SHistoryWindow::GenerateHistoryRow(TSharedPtr<FHistoryItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(STableRow<TSharedPtr<FHistoryItem>>, OwnerTable)
        .Padding(4)
        [
            SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(4, 0, 8, 0)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(Item->ItemType == EHistoryItemType::ImportedAsset ? TEXT("🎨") : TEXT("📄")))
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(SVerticalBox)
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(Item->AssetName))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(Item->FileType))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                        ]
                ]
            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(Item->LastUsed.ToString()))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                ]
        ];
}

void SHistoryWindow::RefreshHistory()
{
    UUERagSubsystem* RAG = GetRagSubsystem();
    if (!RAG) return;

    // 通过 AssetRegistry 获取所有资产
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    TArray<FAssetData> AllAssets;
    AssetRegistry.GetAllAssets(AllAssets);

    AllHistoryItems.Empty();

    for (const FAssetData& Asset : AllAssets)
    {
        TSharedPtr<FHistoryItem> Item = MakeShared<FHistoryItem>();
        Item->AssetName = Asset.AssetName.ToString();
        Item->AssetPath = Asset.GetObjectPathString();
        Item->FileType = Asset.AssetClassPath.GetAssetName().ToString();
        Item->ItemType = EHistoryItemType::ImportedAsset;
        Item->LastUsed = FDateTime::Now(); // 可以后续从元数据获取

        AllHistoryItems.Add(Item);
    }

    ApplyFilter();
}

void SHistoryWindow::ApplyFilter()
{
    if (CurrentFilterType.IsEmpty() || CurrentFilterType == TEXT("全部"))
    {
        HistoryListView->SetItemsSource(&AllHistoryItems);
        return;
    }

    TArray<TSharedPtr<FHistoryItem>> FilteredItems;
    for (const auto& Item : AllHistoryItems)
    {
        if (Item->FileType.Contains(CurrentFilterType))
        {
            FilteredItems.Add(Item);
        }
    }
    HistoryListView->SetItemsSource(&FilteredItems);
}

void SHistoryWindow::OnHistoryItemDoubleClicked(TSharedPtr<FHistoryItem> Item)
{
    if (!Item.IsValid()) return;

    // 在内容浏览器中定位资产 - 需要 FAssetData
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(Item->AssetPath));
    if (AssetData.IsValid())
    {
        FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
        TArray<FAssetData> AssetsToSync;
        AssetsToSync.Add(AssetData);
        ContentBrowserModule.Get().SyncBrowserToAssets(AssetsToSync);
    }

    // 显示资产详细信息
    ShowAssetDetails(Item);
}

void SHistoryWindow::ShowAssetDetails(TSharedPtr<FHistoryItem> Item)
{
    UUERagSubsystem* RAG = GetRagSubsystem();
    if (!RAG) return;

    // 查询该资产的详细信息
    FQueryRequest Request;
    Request.Query = Item->AssetName;
    Request.TopK = 1;

    // 创建静态委托并绑定
    FOnQueryCompleteDelegate Delegate;
    Delegate.BindSP(this, &SHistoryWindow::OnAssetDetailsQueried, Item);

    RAG->QueryRagAsync(Request, Delegate);
}

void SHistoryWindow::OnAssetDetailsQueried(const FQueryResponse& Response, TSharedPtr<FHistoryItem> Item)
{
    if (Response.bSuccess && Response.Sources.Num() > 0)
    {
        ShowAssetInfoWindow(Item, Response.Sources[0]);
    }
}

void SHistoryWindow::ShowAssetInfoWindow(TSharedPtr<FHistoryItem> Item, const FSourceDocument& Document)
{
    // 创建信息窗口
    TSharedRef<SWindow> InfoWindow = SNew(SWindow)
        .Title(FText::FromString(FString::Printf(TEXT("资产信息: %s"), *Item->AssetName)))
        .ClientSize(FVector2D(500, 400))
        .SupportsMinimize(true)
        .SupportsMaximize(false);

    FString InfoText = FString::Printf(
        TEXT("名称: %s\n")
        TEXT("类型: %s\n")
        TEXT("路径: %s\n")
        TEXT("相关文档: %s\n"),
        *Item->AssetName,
        *Item->FileType,
        *Item->AssetPath,
        *Document.Content
    );

    InfoWindow->SetContent(
        SNew(SBorder)
        .Padding(10)
        [
            SNew(SMultiLineEditableTextBox)
                .Text(FText::FromString(InfoText))
                .IsReadOnly(true)
                .AutoWrapText(true)
        ]
    );

    FSlateApplication::Get().AddWindow(InfoWindow);
}

void SHistoryWindow::OnFileTypeFilterChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
    if (Selection.IsValid())
    {
        CurrentFilterType = *Selection;
        ApplyFilter();
    }
}

UUERagSubsystem* SHistoryWindow::GetRagSubsystem() const
{
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