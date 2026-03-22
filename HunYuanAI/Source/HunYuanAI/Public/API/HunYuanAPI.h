#pragma once
#include "CoreMinimal.h"
#include "HunYuanAPITypes.h"
#include "HunYuanModels.h"
#include "Containers/Queue.h"
#include "HAL/ThreadSafeBool.h"
#include "HAL/Runnable.h"
#include "Dom/JsonObject.h"

class FHunYuanAPIImpl;

class HUNYUANAI_API FHunYuanAPI : public TSharedFromThis<FHunYuanAPI>
{
public:
    static TSharedPtr<FHunYuanAPI> Get();
    static void Shutdown();

    // 设置凭证（线程安全）
    void SetCredentials(const FString& InSecretId, const FString& InSecretKey,const FString& InRegion = TEXT("ap-guangzhou"));

    // 异步提交3D生成任务
    void SubmitProJobFromText(const FString& Prompt, HunYuanAPI::EModelFormat Format, FOnJobSubmitted Callback);
    void SubmitProJobFromImage(const TArray<uint8>& ImageData, HunYuanAPI::EModelFormat Format, FOnJobSubmitted Callback);

    // 内部请求方法
    void SendRequestAsync(const FString& Action, const TSharedPtr<FJsonObject>& Params,
        FOnAPIRequestComplete Callback);

    // 查询任务结果
    void QueryProJobResult(const FString& JobId, FOnJobQueried Callback);

    // 取消所有请求
    void CancelAllRequests();

    // 检查API是否就绪
    bool IsReady() const;

    // Tick（用于处理响应）
    void Tick();

    ~FHunYuanAPI();

private:
    FHunYuanAPI();
    static TSharedPtr<FHunYuanAPI> Instance;
    static FCriticalSection InstanceCriticalSection;

    TUniquePtr<FHunYuanAPIImpl> Impl;
    FThreadSafeBool bIsReady;
};