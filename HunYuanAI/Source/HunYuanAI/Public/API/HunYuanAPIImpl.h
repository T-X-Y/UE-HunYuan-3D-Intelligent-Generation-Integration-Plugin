#pragma once

#include "CoreMinimal.h"
#include "HunYuanAPITypes.h"  // 包含委托定义
#include "Containers/Queue.h"
#include "HAL/ThreadSafeBool.h"
#include "HAL/Runnable.h"
#include "Misc/SingleThreadRunnable.h"
#include "Dom/JsonObject.h"
#include <string>
#include <atomic>

namespace TencentCloud
{
    class Credential;
    class CommonClient;
    class HttpProfile;
    class ClientProfile;
}

// 前向声明
class FHunYuanAPIImpl;



class HUNYUANAI_API FHunYuanAPIImpl : public FRunnable, public FSingleThreadRunnable
{
public:
    FHunYuanAPIImpl();
    virtual ~FHunYuanAPIImpl();

    // 设置凭证
    void SetCredentials(const FString& InSecretId, const FString& InSecretKey,
        const FString& InRegion);

    // 发送异步请求
    void SendRequestAsync(const FString& Action, const TSharedPtr<FJsonObject>& Params,
        FOnAPIRequestComplete Callback);

    // 取消所有请求
    void CancelAllRequests();

    // 检查是否有效
    bool IsValid() const { return bIsValid; }

    // FRunnable 接口
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;
    virtual void Exit() override;

    // FSingleThreadRunnable 接口
    virtual void Tick() override;

private:

    // 内部请求结构
    struct FAPIRequest
    {
        FString Action;
        TSharedPtr<FJsonObject> Params;
        FOnAPIRequestComplete Callback;
        FString RequestId;
    };

    // 内部响应结构
    struct FAPIResponse
    {
        FAPIRequest Request;
        TSharedPtr<FJsonObject> Result;
    };

    // 处理请求的线程函数
    void ProcessRequests();

    // 执行单个请求
    bool ExecuteRequest(const FAPIRequest& Request, TSharedPtr<FJsonObject>& OutResult);

    // 创建客户端
    bool CreateClient();

    //状态检查
    void CheckStatus() const;

private:
    FThreadSafeBool bRunning;
    FThreadSafeBool bIsValid;

    // 线程安全的队列
    TQueue<FAPIRequest, EQueueMode::Mpsc> RequestQueue;
    TQueue<FAPIResponse, EQueueMode::Mpsc> ResponseQueue;

    // 客户端相关
    FCriticalSection ClientCriticalSection;
    TSharedPtr<TencentCloud::CommonClient> Client;

    // 凭证信息
    FString SecretId;
    FString SecretKey;
    FString Region;
    std::string Service = "ai3d";
    std::string Version = "2025-05-13";

    // 工作线程
    FRunnableThread* WorkerThread;

    // 存储 SDK 对象的成员变量，确保生命周期
    TSharedPtr<TencentCloud::Credential> CredentialPtr;
    TSharedPtr<TencentCloud::HttpProfile> HttpProfilePtr;
    TSharedPtr<TencentCloud::ClientProfile> ClientProfilePtr;

    
};