#pragma once
#include "CoreMinimal.h"
#include "HunYuanAPITypes.h"
#include "HunYuanModels.h"
#include "Containers/Queue.h"
#include "HAL/ThreadSafeBool.h"

class FHunYuanAPIImpl;

// API 请求委托
DECLARE_DELEGATE_OneParam(FOnAPIRequestComplete, const TSharedPtr<FJsonObject>& /* Result */);

class HUNYUANAI_API FHunYuanAPI : public TSharedFromThis<FHunYuanAPI>
{
public:
    static TSharedPtr<FHunYuanAPI> Get();
    static void Shutdown();

    ~FHunYuanAPI();

    // 设置凭证（线程安全）
    void SetCredentials(const FString& InSecretId, const FString& InSecretKey,
        const FString& InRegion = TEXT("ap-guangzhou"));

    // 异步提交3D生成任务
    void SubmitProJobFromText(const FString& Prompt, FOnJobSubmitted Callback);
    void SubmitProJobFromImage(const TArray<uint8>& ImageData, FOnJobSubmitted Callback);

    // 异步查询任务结果
    void QueryProJobResult(const FString& JobId, FOnJobQueried Callback);

    // 取消所有请求
    void CancelAllRequests();

    // 检查API是否就绪
    bool IsReady() const;

    // 每帧调用
    void Tick();

private:
    FHunYuanAPI();

    // 内部请求方法
    void SendRequestAsync(const FString& Action, const TSharedPtr<FJsonObject>& Params,
        FOnAPIRequestComplete Callback);

    static TSharedPtr<FHunYuanAPI> Instance;
    static FCriticalSection InstanceCriticalSection;

    TUniquePtr<FHunYuanAPIImpl> Impl;
    FThreadSafeBool bIsReady;
};