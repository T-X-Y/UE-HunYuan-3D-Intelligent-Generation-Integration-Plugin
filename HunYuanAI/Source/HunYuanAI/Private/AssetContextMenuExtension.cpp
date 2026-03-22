// HunYuanAI/Private/AssetContextMenuExtension.cpp
// 扩展资产右键菜单，添加"查看知识图谱信息"选项

class FAssetContextMenuExtension
{
public:
    static void Register();
    static void Unregister();

private:
    static void AddMenuExtension(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets);
    static void OnViewKnowledgeGraphInfo(TArray<FAssetData> SelectedAssets);

    static FDelegateHandle MenuExtenderHandle;
};