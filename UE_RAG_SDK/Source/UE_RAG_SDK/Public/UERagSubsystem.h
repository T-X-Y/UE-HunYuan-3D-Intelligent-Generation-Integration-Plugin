// UERagSubsystem.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/IHttpRequest.h"
#include "RagTypes.h"
#include "UERagSubsystem.generated.h"

// 前向声明
class URagHttpClient;
class UResourceMonitor;

/**
 * RAG全局子系统
 */
UCLASS(BlueprintType)
class UE_RAG_SDK_API UUERagSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // 配置
    UFUNCTION(BlueprintCallable, Category = "RAG|Config")
    void SetServiceUrl(const FString& Url);

    UFUNCTION(BlueprintCallable, Category = "RAG|Config")
    FString GetServiceUrl() const { return ServiceUrl; }

    // ============ 查询接口 ============

    /** C++可用的异步查询（通过静态委托） */
    void QueryRagAsync(const FQueryRequest& Request, FOnQueryCompleteDelegate InDelegate);

    // ============ 资源导入 ============

    UFUNCTION(BlueprintCallable, Category = "RAG|Resource")
    void ImportResource(const FResourceMetadata& Metadata, UObject* ResourceAsset = nullptr);

    UFUNCTION(BlueprintCallable, Category = "RAG|Resource")
    void ImportResources(const TArray<FResourceMetadata>& MetadataList);

    // ============ 实时更新 ============

    UFUNCTION(BlueprintCallable, Category = "RAG|Realtime")
    void StartResourceMonitoring();

    UFUNCTION(BlueprintCallable, Category = "RAG|Realtime")
    void NotifyResourceChanged(EResourceEventType EventType, const FResourceMetadata& Resource);

    // ============ 蓝图事件 ============

    UPROPERTY(BlueprintAssignable, Category = "RAG|Events")
    FOnQueryCompleted OnQueryCompleted;

    UPROPERTY(BlueprintAssignable, Category = "RAG|Events")
    FOnResourceUpdated OnResourceAdded;

    UPROPERTY(BlueprintAssignable, Category = "RAG|Events")
    FOnResourceUpdated OnResourceUpdated;

    UPROPERTY(BlueprintAssignable, Category = "RAG|Events")
    FOnResourceUpdated OnResourceDeleted;

private:
    UPROPERTY()
    URagHttpClient* HttpClient;

    UPROPERTY()
    UResourceMonitor* ResourceMonitor;

    UPROPERTY()
    FString ServiceUrl = TEXT("http://localhost:8000");

    TMap<FString, FQueryResponse> QueryCache;

    void HandleQueryResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully,
        FString CacheKey, FOnQueryCompleteDelegate InDelegate);

    FQueryResponse ParseQueryResponse(TSharedPtr<FJsonObject> JsonObj);

    void HandleImportResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);
    void HandleUpdateResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

    FString GetCacheKey(const FQueryRequest& Request);

    void LoadCache();
    void SaveCache();
    bool ExportAssetInfo(UObject* Asset, const FString& OutputPath);
};