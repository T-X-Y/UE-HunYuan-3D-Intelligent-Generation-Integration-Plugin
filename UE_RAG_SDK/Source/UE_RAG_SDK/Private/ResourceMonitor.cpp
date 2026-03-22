#include "ResourceMonitor.h"
#include "Engine/AssetManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UResourceMonitor::StartMonitoring()
{
    if (!RagSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("ResourceMonitor: No RagSubsystem set"));
        return;
    }

    // 获取AssetRegistry
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    // 缓存现有资产
    TArray<FAssetData> AllAssets;
    AssetRegistry.GetAllAssets(AllAssets);

    for (const FAssetData& Asset : AllAssets)
    {
        CachedAssets.Add(Asset.ObjectPath, Asset);

        // 记录最后修改时间
        FString FilePath;
        if (FPackageName::DoesPackageExist(Asset.PackageName.ToString(), &FilePath))
        {
            FDateTime ModTime = IFileManager::Get().GetTimeStamp(*FilePath);
            LastModifiedTimes.Add(Asset.ObjectPath.ToString(), ModTime);
        }
    }

    // 绑定事件
    if (bMonitorOnSave)
    {
        UPackage::PreSavePackageEvent.AddUObject(this, &UResourceMonitor::OnPackagePreSave);
        UPackage::PackageSavedEvent.AddUObject(this, &UResourceMonitor::OnPackageSaved);
    }

    AssetRegistry.OnAssetAdded().AddUObject(this, &UResourceMonitor::OnAssetAdded);
    AssetRegistry.OnAssetRemoved().AddUObject(this, &UResourceMonitor::OnAssetRemoved);
    AssetRegistry.OnAssetRenamed().AddUObject(this, &UResourceMonitor::OnAssetRenamed);
    AssetRegistry.OnAssetUpdated().AddUObject(this, &UResourceMonitor::OnAssetUpdated);

    // 启动定时扫描
    if (bPeriodicScan)
    {
        GetWorld()->GetTimerManager().SetTimer(
            ScanTimerHandle,
            this,
            &UResourceMonitor::ScanForChanges,
            ScanInterval,
            true
        );
    }

    // 启动延迟通知定时器
    GetWorld()->GetTimerManager().SetTimer(
        DeferredNotificationTimer,
        this,
        &UResourceMonitor::FlushPendingNotifications,
        1.0f,
        true
    );

    UE_LOG(LogTemp, Log, TEXT("ResourceMonitor started with %d cached assets"), CachedAssets.Num());
}

void UResourceMonitor::OnAssetAdded(const FAssetData& NewAsset)
{
    // 检查是否应该监控
    if (!ShouldMonitorAsset(NewAsset))
        return;

    FString AssetPath = NewAsset.ObjectPath.ToString();

    // 避免重复处理
    if (ProcessingAssets.Contains(AssetPath))
        return;

    ProcessingAssets.Add(AssetPath);

    // 检查是新增还是更新
    if (CachedAssets.Contains(NewAsset.ObjectPath))
    {
        // 更新
        FResourceMetadata Metadata = GenerateMetadataFromAssetData(NewAsset);
        QueueNotification(EResourceEventType::Update, Metadata);
    }
    else
    {
        // 新增
        CachedAssets.Add(NewAsset.ObjectPath, NewAsset);
        FResourceMetadata Metadata = GenerateMetadataFromAssetData(NewAsset);
        QueueNotification(EResourceEventType::Add, Metadata);
    }

    ProcessingAssets.Remove(AssetPath);
}

void UResourceMonitor::OnAssetRemoved(const FAssetData& RemovedAsset)
{
    FString AssetPath = RemovedAsset.ObjectPath.ToString();

    if (!CachedAssets.Contains(RemovedAsset.ObjectPath))
        return;

    // 从缓存中删除
    CachedAssets.Remove(RemovedAsset.ObjectPath);
    LastModifiedTimes.Remove(AssetPath);

    // 通知删除
    FResourceMetadata Metadata;
    Metadata.ResourceId = FMD5::HashAnsiString(*AssetPath);
    Metadata.ResourceName = RemovedAsset.AssetName.ToString();
    Metadata.ResourcePath = AssetPath;
    Metadata.ResourceType = GetAssetType(RemovedAsset);

    QueueNotification(EResourceEventType::Delete, Metadata);
}

void UResourceMonitor::ScanForChanges()
{
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    TArray<FAssetData> CurrentAssets;
    AssetRegistry.GetAllAssets(CurrentAssets);

    // 构建当前资产映射
    TMap<FName, FAssetData> CurrentAssetMap;
    for (const FAssetData& Asset : CurrentAssets)
    {
        CurrentAssetMap.Add(Asset.ObjectPath, Asset);
    }

    // 检查删除的资产
    TArray<FName> RemovedPaths;
    for (const auto& Pair : CachedAssets)
    {
        if (!CurrentAssetMap.Contains(Pair.Key))
        {
            RemovedPaths.Add(Pair.Key);
        }
    }

    for (FName RemovedPath : RemovedPaths)
    {
        FAssetData RemovedAsset = CachedAssets[RemovedPath];
        OnAssetRemoved(RemovedAsset);
    }

    // 检查新增/更新的资产
    for (const auto& Pair : CurrentAssetMap)
    {
        const FAssetData& Asset = Pair.Value;

        // 检查是否应该监控
        if (!ShouldMonitorAsset(Asset))
            continue;

        FString AssetPath = Asset.ObjectPath.ToString();

        // 检查是否是新增
        if (!CachedAssets.Contains(Asset.ObjectPath))
        {
            OnAssetAdded(Asset);
            continue;
        }

        // 检查是否有修改（通过时间戳）
        FString FilePath;
        if (FPackageName::DoesPackageExist(Asset.PackageName.ToString(), &FilePath))
        {
            FDateTime ModTime = IFileManager::Get().GetTimeStamp(*FilePath);

            if (LastModifiedTimes.Contains(AssetPath) && LastModifiedTimes[AssetPath] != ModTime)
            {
                // 文件已修改
                LastModifiedTimes[AssetPath] = ModTime;
                OnAssetAdded(Asset);
            }
        }
    }
}

bool UResourceMonitor::ShouldMonitorAsset(const FAssetData& Asset)
{
    // 检查排除路径
    FString AssetPath = Asset.ObjectPath.ToString();
    for (const FString& Excluded : ExcludedPaths)
    {
        if (AssetPath.StartsWith(Excluded))
            return false;
    }

    // 如果指定了监控路径
    if (MonitoredPaths.Num() > 0)
    {
        bool bInMonitoredPath = false;
        for (const FString& Path : MonitoredPaths)
        {
            if (AssetPath.StartsWith(Path))
            {
                bInMonitoredPath = true;
                break;
            }
        }
        if (!bInMonitoredPath)
            return false;
    }

    // 检查资产类型
    if (MonitoredAssetClasses.Num() > 0)
    {
        UClass* AssetClass = Asset.GetClass();
        if (!AssetClass)
            return false;

        bool bClassMatched = false;
        for (UClass* MonitoredClass : MonitoredAssetClasses)
        {
            if (AssetClass->IsChildOf(MonitoredClass))
            {
                bClassMatched = true;
                break;
            }
        }
        if (!bClassMatched)
            return false;
    }

    return true;
}

void UResourceMonitor::QueueNotification(EResourceEventType EventType, const FResourceMetadata& Metadata)
{
    // 去重：如果队列中已有相同资源的相同事件，跳过
    for (const auto& Pending : PendingNotifications)
    {
        if (Pending.ResourceId == Metadata.ResourceId)
        {
            // 如果是删除事件，保留删除（删除优先级最高）
            if (EventType == EResourceEventType::Delete)
            {
                // 移除旧的，添加新的删除
                PendingNotifications.RemoveAll([&](const FResourceMetadata& M) {
                    return M.ResourceId == Metadata.ResourceId;
                    });
                break;
            }
            else
            {
                // 已有相同资源，跳过
                return;
            }
        }
    }

    PendingNotifications.Add(Metadata);
}

void UResourceMonitor::FlushPendingNotifications()
{
    if (PendingNotifications.Num() == 0 || !RagSubsystem)
        return;

    // 批量发送 - 临时注释掉，因为 NotifyResourcesChanged 需要实现
    // RagSubsystem->NotifyResourcesChanged(PendingNotifications);
    PendingNotifications.Empty();
}

void UResourceMonitor::Initialize(UUERagSubsystem* InSubsystem)
{
    RagSubsystem = InSubsystem;
}

void UResourceMonitor::StopMonitoring()
{
    // 清理定时器
    if (ScanTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ScanTimerHandle);
    }
    if (DeferredNotificationTimer.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(DeferredNotificationTimer);
    }

    // 解绑事件
    UnbindEngineCallbacks();
}

void UResourceMonitor::MonitorAsset(UObject* Asset)
{
    if (!Asset) return;

    FAssetData AssetData(Asset);
    if (ShouldMonitorAsset(AssetData))
    {
        OnAssetAdded(AssetData);
    }
}

void UResourceMonitor::MonitorPath(const FString& AssetPath)
{
    MonitoredPathsSet.Add(AssetPath);
}

void UResourceMonitor::OnAssetRenamed(const FAssetData& Asset, const FString& OldPath)
{
    FString AssetPath = Asset.ObjectPath.ToString();

    if (CachedAssets.Contains(FName(*OldPath)))
    {
        CachedAssets.Remove(FName(*OldPath));
        CachedAssets.Add(Asset.ObjectPath, Asset);
    }

    if (LastModifiedTimes.Contains(OldPath))
    {
        FDateTime ModTime = LastModifiedTimes[OldPath];
        LastModifiedTimes.Remove(OldPath);
        LastModifiedTimes.Add(AssetPath, ModTime);
    }

    FResourceMetadata Metadata = GenerateMetadataFromAssetData(Asset);
    QueueNotification(EResourceEventType::Update, Metadata);
}

void UResourceMonitor::OnAssetUpdated(const FAssetData& Asset)
{
    OnAssetAdded(Asset);
}

void UResourceMonitor::OnPackagePreSave(UPackage* Package)
{
    // 包保存前的处理
}

void UResourceMonitor::OnPackageSaved(const FString& PackageFilename, UObject* Outer)
{
    if (Outer)
    {
        FAssetData AssetData(Outer);
        if (ShouldMonitorAsset(AssetData))
        {
            OnAssetAdded(AssetData);
        }
    }
}

FResourceMetadata UResourceMonitor::GenerateMetadataFromAssetData(const FAssetData& Asset)
{
    FResourceMetadata Metadata;
    Metadata.ResourceId = FMD5::HashAnsiString(*Asset.ObjectPath.ToString());
    Metadata.ResourceName = Asset.AssetName.ToString();
    Metadata.ResourcePath = Asset.ObjectPath.ToString();
    Metadata.ResourceType = GetAssetType(Asset);
    Metadata.Tags.Add(Asset.AssetClass.ToString());
    return Metadata;
}

FString UResourceMonitor::GetAssetType(const FAssetData& Asset)
{
    FString ClassName = Asset.AssetClass.ToString();

    if (ClassName == TEXT("StaticMesh"))
        return TEXT("mesh");
    else if (ClassName == TEXT("Texture2D"))
        return TEXT("texture");
    else if (ClassName == TEXT("Material"))
        return TEXT("material");
    else if (ClassName == TEXT("Blueprint"))
        return TEXT("blueprint");
    else if (ClassName == TEXT("AnimSequence"))
        return TEXT("animation");
    else if (ClassName == TEXT("SkeletalMesh"))
        return TEXT("skeletal_mesh");

    return ClassName.ToLower();
}

void UResourceMonitor::BindEngineCallbacks()
{
    // 绑定已在 StartMonitoring 中实现
}

void UResourceMonitor::UnbindEngineCallbacks()
{
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    AssetRegistry.OnAssetAdded().RemoveAll(this);
    AssetRegistry.OnAssetRemoved().RemoveAll(this);
    AssetRegistry.OnAssetRenamed().RemoveAll(this);
    AssetRegistry.OnAssetUpdated().RemoveAll(this);

    if (bMonitorOnSave)
    {
        UPackage::PreSavePackageEvent.RemoveAll(this);
        UPackage::PackageSavedEvent.RemoveAll(this);
    }
}