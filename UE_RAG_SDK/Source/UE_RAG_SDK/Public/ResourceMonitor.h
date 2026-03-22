#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RagTypes.h"
#include "ResourceMonitor.generated.h"

// 前向声明
class UUERagSubsystem;

/**
 * 监控UE资源变化并触发RAG更新
 */
UCLASS(BlueprintType)
class UE_RAG_SDK_API UResourceMonitor : public UObject
{
    GENERATED_BODY()

public:
    // 初始化
    void Initialize(UUERagSubsystem* InSubsystem);
    void StartMonitoring();
    void StopMonitoring();


    // 监控模式
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAG|Monitor")
    bool bMonitorAllAssets = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAG|Monitor")
    bool bMonitorOnSave = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAG|Monitor")
    bool bPeriodicScan = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAG|Monitor")
    float ScanInterval = 5.0f;

    // 监控过滤器
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAG|Monitor|Filter")
    TArray<UClass*> MonitoredAssetClasses;  // 如 UStaticMesh::StaticClass()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAG|Monitor|Filter")
    TArray<FString> MonitoredPaths;  // 如 "/Game/Characters"

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RAG|Monitor|Filter")
    TArray<FString> ExcludedPaths;  // 如 "/Game/Developers"

    // 手动添加监控目标
    UFUNCTION(BlueprintCallable, Category = "RAG|Monitor")
    void MonitorAsset(UObject* Asset);

    UFUNCTION(BlueprintCallable, Category = "RAG|Monitor")
    void MonitorPath(const FString& AssetPath);

private:
    // RAG子系统引用
    UPROPERTY()
    UUERagSubsystem* RagSubsystem;

    // 资产注册表缓存
    TMap<FName, FAssetData> CachedAssets;

    // 上次修改时间缓存
    TMap<FString, FDateTime> LastModifiedTimes;

    // 正在处理的变更（避免循环）
    TSet<FString> ProcessingAssets;

    // 延迟通知队列
    TArray<FResourceMetadata> PendingNotifications;

    // 定时器句柄
    FTimerHandle ScanTimerHandle;
    FTimerHandle DeferredNotificationTimer;

    // 已注册的监控路径
    TSet<FString> MonitoredPathsSet;

    // 私有方法
    void OnAssetAdded(const FAssetData& NewAsset);
    void OnAssetRemoved(const FAssetData& RemovedAsset);
    void OnAssetRenamed(const FAssetData& Asset, const FString& OldPath);
    void OnAssetUpdated(const FAssetData& Asset);
    void OnPackagePreSave(UPackage* Package);
    void OnPackageSaved(const FString& PackageFilename, UObject* Outer);

    void ScanForChanges();
    bool ShouldMonitorAsset(const FAssetData& Asset);
    FResourceMetadata GenerateMetadataFromAssetData(const FAssetData& Asset);
    FString GetAssetType(const FAssetData& Asset);

    void QueueNotification(EResourceEventType EventType, const FResourceMetadata& Metadata);
    void FlushPendingNotifications();

    void BindEngineCallbacks();
    void UnbindEngineCallbacks();
};