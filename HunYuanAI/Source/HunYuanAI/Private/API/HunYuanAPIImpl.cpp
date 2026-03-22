// API/HunYuanAPIImpl.cpp

#include "API/HunYuanAPIImpl.h"
#include "Logging/HunYuanLogging.h"
#include "Async/Async.h"

// 腾讯云SDK头文件（根据实际路径调整）
#include "tencentcloud/core/TencentCloud.h"
#include "tencentcloud/core/Credential.h"
#include "tencentcloud/core/CommonClient.h"
#include "tencentcloud/core/profile/HttpProfile.h"
#include "tencentcloud/core/profile/ClientProfile.h"


using namespace TencentCloud;


// 内部请求响应类
class FHunYuanRequest : public TencentCloud::AbstractModel
{
public:
    FHunYuanRequest(const FString& InAction, const TSharedPtr<FJsonObject>& InParams)
        : Action(TCHAR_TO_UTF8(*InAction))
        , Params(InParams)
    {
    }

    ~FHunYuanRequest() {}

    virtual std::string ToJsonString() const override
    {
        if (!Params.IsValid())
        {
            return "{}";
        }

        FString JsonString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
        FJsonSerializer::Serialize(Params.ToSharedRef(), Writer);
        return std::string(TCHAR_TO_UTF8(*JsonString));
    }

private:
    std::string Action;
    TSharedPtr<FJsonObject> Params;
};

class FHunYuanResponse : public TencentCloud::AbstractModel
{
public:
    FHunYuanResponse()
        : Result(MakeShareable(new FJsonObject()))
    {
    }

    ~FHunYuanResponse() {}

    TencentCloud::CoreInternalOutcome Deserialize(const std::string& payload)
    {
        FString JsonString = UTF8_TO_TCHAR(payload.c_str());
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

        TSharedPtr<FJsonObject> RootObject;
        if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
        {
            return TencentCloud::CoreInternalOutcome(Core::Error("InvalidJson", "Failed to parse response JSON"));
        }

        // 检查错误
        FString ErrorCode, ErrorMessage;
        const TSharedPtr<FJsonObject>* ErrorObject = nullptr;
        if (RootObject->TryGetObjectField(TEXT("Error"), ErrorObject))
        {
            (*ErrorObject)->TryGetStringField(TEXT("Code"), ErrorCode);
            (*ErrorObject)->TryGetStringField(TEXT("Message"), ErrorMessage);
            if (!ErrorCode.IsEmpty())
            {
                return TencentCloud::CoreInternalOutcome(
                    Core::Error(std::string(TCHAR_TO_UTF8(*ErrorCode)),
                        std::string(TCHAR_TO_UTF8(*ErrorMessage))));
            }
        }

        // 获取RequestId
        FString RequestId;
        RootObject->TryGetStringField(TEXT("RequestId"), RequestId);
        SetRequestId(std::string(TCHAR_TO_UTF8(*RequestId)));

        // 获取Response对象
        const TSharedPtr<FJsonObject>* ResponseObject = nullptr;
        if (RootObject->TryGetObjectField(TEXT("Response"), ResponseObject))
        {
            Result = *ResponseObject;
        }
        else
        {
            Result = RootObject;
        }

        return TencentCloud::CoreInternalOutcome(true);
    }

    TSharedPtr<FJsonObject> GetResult() const { return Result; }

private:
    TSharedPtr<FJsonObject> Result;
};

// FHunYuanAPIImpl 实现
FHunYuanAPIImpl::FHunYuanAPIImpl()
    : bRunning(false)
    , bIsValid(false)
    , bNeedsClientRecreation(false)
    , WorkerThread(nullptr)
    , Client(nullptr)
    , CredentialPtr(nullptr)
    , HttpProfilePtr(nullptr)
    , ClientProfilePtr(nullptr)
    ,Service("ai3d")
    ,Version("2025-05-13")
{
    TencentCloud::InitAPI();
}

FHunYuanAPIImpl::~FHunYuanAPIImpl()
{
    UE_LOG(LogHunYuanAPI, Log, TEXT("FHunYuanAPIImpl destructor started"));

    // 1. 先停止接受新请求
    bRunning = false;

    // 2. 清空队列，避免线程还在处理
    FAPIRequest Request;
    while (RequestQueue.Dequeue(Request)) {}

    FAPIResponse Response;
    while (ResponseQueue.Dequeue(Response)) {}

    // 3. 等待线程结束
    if (WorkerThread)
    {
        WorkerThread->WaitForCompletion();
        delete WorkerThread;
        WorkerThread = nullptr;
    }

    // 3. 等待线程结束
    if (WorkerThread)
    {
        WorkerThread->WaitForCompletion();
        delete WorkerThread;
        WorkerThread = nullptr;
    }

    // 4. 先清理 Client（重要：在 ShutdownAPI 之前）
    {
        FScopeLock Lock(&ClientCriticalSection);

        // 先重置 Client，这会触发 lambda 删除器
        // 删除器会删除 RawClient，但不会立即释放 libcurl 资源
        if (Client.IsValid())
        {
            // 获取原始指针，准备手动清理
            TencentCloud::CommonClient* RawClient = Client.Get();
            Client.Reset();  // 先 Reset，让 lambda 删除器执行
        }

        // 再重置依赖对象
        CredentialPtr.Reset();
        HttpProfilePtr.Reset();
        ClientProfilePtr.Reset();
    }
    // 5. 给 libcurl 一些时间完成清理
    FPlatformProcess::Sleep(0.1f);

    // 6. 最后关闭 SDK
    TencentCloud::ShutdownAPI();

    UE_LOG(LogHunYuanAPI, Log, TEXT("FHunYuanAPIImpl destructor completed"));
}

void FHunYuanAPIImpl::SetCredentials(const FString& InSecretId, const FString& InSecretKey, const FString& InRegion)
{
    FScopeLock Lock(&ClientCriticalSection);

    SecretId = InSecretId;
    SecretKey = InSecretKey;
    Region = InRegion.IsEmpty() ? TEXT("ap-guangzhou") : InRegion;

    // 改为标记客户端为"脏"，下次请求时重新创建
    bNeedsClientRecreation = true;

    bIsValid = true;

    UE_LOG(LogHunYuanAPI, Log, TEXT("Credentials updated, client will be recreated on next request"));
}

void FHunYuanAPIImpl::SendRequestAsync(const FString& Action, const TSharedPtr<FJsonObject>& Params,
    FOnAPIRequestComplete Callback)
{
    // 生成唯一请求ID用于跟踪
    static int32 RequestCounter = 0;
    FString RequestId = FString::Printf(TEXT("Req_%d_%s"), ++RequestCounter, *FDateTime::Now().ToString(TEXT("%H%M%S")));

    UE_LOG(LogHunYuanAI, Log, TEXT("[%s] SendRequestAsync started - Action: %s"), *RequestId, *Action);

    if (!bRunning)
    {
        bRunning = true;
        WorkerThread = FRunnableThread::Create(this, TEXT("HunYuanAPIThread"));
    }

    FAPIRequest Request;
    Request.Action = Action;
    Request.Params = Params;
    Request.Callback = Callback;
    Request.RequestId = RequestId;

    RequestQueue.Enqueue(MoveTemp(Request));
}

bool FHunYuanAPIImpl::Init()
{
    UE_LOG(LogHunYuanAI, Log, TEXT("API Worker Thread Initialized"));
    return true;
}

uint32 FHunYuanAPIImpl::Run()
{
    while (bRunning)
    {
        ProcessRequests();
        FPlatformProcess::Sleep(0.01f); // 避免CPU占用过高
    }
    return 0;
}

void FHunYuanAPIImpl::Stop()
{
    bRunning = false;
}

void FHunYuanAPIImpl::Exit()
{
    UE_LOG(LogHunYuanAI, Log, TEXT("API Worker Thread Exited"));

    // 清理队列
    FAPIRequest Request;
    while (RequestQueue.Dequeue(Request))
    {
        // 发送空结果
        AsyncTask(ENamedThreads::GameThread, [Callback = Request.Callback]()
            {
                Callback.ExecuteIfBound(nullptr);
            });
    }

    FAPIResponse Response;
    while (ResponseQueue.Dequeue(Response))
    {
        AsyncTask(ENamedThreads::GameThread, [Response]()
            {
                Response.Request.Callback.ExecuteIfBound(Response.Result);
            });
    }
}

void FHunYuanAPIImpl::Tick()
{
    // 确保在游戏线程调用
    if (!IsInGameThread())
    {
        // 如果在非游戏线程，调度到游戏线程
        AsyncTask(ENamedThreads::GameThread, [this]()
            {
                Tick();
            });
        UE_LOG(LogHunYuanAI, Warning, TEXT("Tick called from non-game thread"));
        return;
    }

    // 处理响应队列（在游戏线程）
    int32 ProcessedCount = 0;
    FAPIResponse Response;
    while (ResponseQueue.Dequeue(Response))
    {
        ProcessedCount++;
        UE_LOG(LogHunYuanAI, Log, TEXT("[%s] Tick processing response - Success: %d"),
            *Response.Request.RequestId, Response.Result.IsValid());

        if (Response.Request.Callback.IsBound())
        {
            UE_LOG(LogHunYuanAI, Log, TEXT("[%s] Executing callback"), *Response.Request.RequestId);
            Response.Request.Callback.ExecuteIfBound(Response.Result);
        }
        else
        {
            UE_LOG(LogHunYuanAI, Warning, TEXT("[%s] Callback not bound!"), *Response.Request.RequestId);
        }
    }

    if (ProcessedCount > 0)
    {
        UE_LOG(LogHunYuanAI, Log, TEXT("Tick processed %d responses"), ProcessedCount);
    }
}

void FHunYuanAPIImpl::ProcessRequests()
{
    FAPIRequest Request;
    if (RequestQueue.Dequeue(Request))
    {
        UE_LOG(LogHunYuanAI, Log, TEXT("[%s] Processing request - Action: %s"),
            *Request.RequestId, *Request.Action);
        TSharedPtr<FJsonObject> Result;
        bool bSuccess = ExecuteRequest(Request, Result);

        UE_LOG(LogHunYuanAI, Log, TEXT("[%s] Request executed - Success: %d, Result: %s"),
            *Request.RequestId, bSuccess, Result.IsValid() ? TEXT("valid") : TEXT("null"));

        FAPIResponse Response;
        Response.Request = Request;
        Response.Result = Result;
        ResponseQueue.Enqueue(MoveTemp(Response));

        UE_LOG(LogHunYuanAI, Log, TEXT("[%s] Response queued - Queue is %s"),*Request.RequestId,ResponseQueue.IsEmpty() ? TEXT("empty") : TEXT("not empty"));
    }
}

bool FHunYuanAPIImpl::ExecuteRequest(const FAPIRequest& Request, TSharedPtr<FJsonObject>& OutResult)
{
    UE_LOG(LogHunYuanAI, Log, TEXT("[%s] ExecuteRequest started"), *Request.RequestId);

    // 检查委托是否绑定
    if (!Request.Callback.IsBound())
    {
        UE_LOG(LogHunYuanAI, Warning, TEXT("[%s] Callback not bound"), *Request.RequestId);
        OutResult = MakeShareable(new FJsonObject());
        OutResult->SetStringField(TEXT("ErrorCode"), TEXT("CallbackNotBound"));
        OutResult->SetStringField(TEXT("ErrorMessage"), TEXT("Callback is not bound"));
        return false;
    }

    // ===== 检查是否需要重建客户端 =====
    {
        FScopeLock Lock(&ClientCriticalSection);
        if (bNeedsClientRecreation)
        {
            UE_LOG(LogHunYuanAI, Log, TEXT("[%s] Client needs recreation due to credential change"), *Request.RequestId);
            Client.Reset();
            bNeedsClientRecreation = false;
        }
    }

    if (!CreateClient())
    {
        UE_LOG(LogHunYuanAI, Error, TEXT("[%s] Failed to create API client"), *Request.RequestId);
        OutResult = MakeShareable(new FJsonObject());
        OutResult->SetStringField(TEXT("ErrorCode"), TEXT("ClientCreationFailed"));
        OutResult->SetStringField(TEXT("ErrorMessage"), TEXT("Failed to create API client"));
        return false;
    }

    TSharedPtr<TencentCloud::CommonClient> ClientCopy;
    {
        FScopeLock Lock(&ClientCriticalSection);
        ClientCopy = Client;
        UE_LOG(LogHunYuanAI, Log, TEXT("[%s] Got client copy - Valid: %d"),
            *Request.RequestId, ClientCopy.IsValid());
    }

    if (!ClientCopy.IsValid())
    {
        UE_LOG(LogHunYuanAI, Error, TEXT("[%s] API client is invalid"), *Request.RequestId);
        OutResult = MakeShareable(new FJsonObject());
        OutResult->SetStringField(TEXT("ErrorCode"), TEXT("InvalidClient"));
        OutResult->SetStringField(TEXT("ErrorMessage"), TEXT("API client is invalid"));
        return false;
    }

    try
    {
        UE_LOG(LogHunYuanAI, Log, TEXT("[%s] Creating request object"), *Request.RequestId);
        auto Req = MakeShared<FHunYuanRequest>(Request.Action, Request.Params);
        std::string RequestBody = Req->ToJsonString();

        UE_LOG(LogHunYuanAI, Verbose, TEXT("[%s] Sending request with body: %s"),
            *Request.RequestId, UTF8_TO_TCHAR(RequestBody.c_str()));

        UE_LOG(LogHunYuanAI, Log, TEXT("[%s] Calling MakeRequestJson..."), *Request.RequestId);
        auto httpOutcome = ClientCopy->MakeRequestJson(
            std::string(TCHAR_TO_UTF8(*Request.Action)),
            RequestBody
        );

        UE_LOG(LogHunYuanAI, Log, TEXT("[%s] MakeRequestJson completed - Success: %d"),
            *Request.RequestId, httpOutcome.IsSuccess());

        if (httpOutcome.IsSuccess())
        {
            UE_LOG(LogHunYuanAI, Log, TEXT("[%s] Parsing response..."), *Request.RequestId);

            FHunYuanResponse resp;
            auto deserializeOutcome = resp.Deserialize(httpOutcome.GetResult().Body());

            if (deserializeOutcome.IsSuccess())
            {
                OutResult = resp.GetResult();
                UE_LOG(LogHunYuanAI, Log, TEXT("[%s] Request succeeded"), *Request.RequestId);
                return true;
            }
            else
            {
                UE_LOG(LogHunYuanAI, Error, TEXT("[%s] Failed to deserialize response"), *Request.RequestId);
                OutResult = MakeShareable(new FJsonObject());
                OutResult->SetStringField(TEXT("ErrorCode"), TEXT("DeserializeFailed"));
                OutResult->SetStringField(TEXT("ErrorMessage"), TEXT("Failed to deserialize response"));
            }
        }
        else
        {
            Core::Error error = httpOutcome.GetError();
            UE_LOG(LogHunYuanAI, Error, TEXT("[%s] Request failed: %s"),
                *Request.RequestId, UTF8_TO_TCHAR(error.GetErrorMessage().c_str()));

            OutResult = MakeShareable(new FJsonObject());
            OutResult->SetStringField(TEXT("ErrorCode"), UTF8_TO_TCHAR(error.GetErrorCode().c_str()));
            OutResult->SetStringField(TEXT("ErrorMessage"), UTF8_TO_TCHAR(error.GetErrorMessage().c_str()));
        }
    }
    catch (const std::exception& e)
    {
        UE_LOG(LogHunYuanAI, Error, TEXT("[%s] Exception in API request: %s"),
            *Request.RequestId, UTF8_TO_TCHAR(e.what()));

        OutResult = MakeShareable(new FJsonObject());
        OutResult->SetStringField(TEXT("ErrorCode"), TEXT("Exception"));
        OutResult->SetStringField(TEXT("ErrorMessage"), UTF8_TO_TCHAR(e.what()));
    }
    catch (...)
    {
        UE_LOG(LogHunYuanAI, Error, TEXT("[%s] Unknown exception in API request"), *Request.RequestId);

        OutResult = MakeShareable(new FJsonObject());
        OutResult->SetStringField(TEXT("ErrorCode"), TEXT("UnknownException"));
        OutResult->SetStringField(TEXT("ErrorMessage"), TEXT("Unknown exception occurred"));
    }

    return false;
}

bool FHunYuanAPIImpl::CreateClient()
{
    FScopeLock Lock(&ClientCriticalSection);

    if (Client.IsValid())
    {
        return true;
    }

    if (SecretId.IsEmpty() || SecretKey.IsEmpty())
    {
        UE_LOG(LogHunYuanAPI, Error, TEXT("SecretId or SecretKey is empty"));
        return false;
    }

    try
    {
        std::string AnsiSecretId = TCHAR_TO_UTF8(*SecretId);
        std::string AnsiSecretKey = TCHAR_TO_UTF8(*SecretKey);
        std::string AnsiRegion = TCHAR_TO_UTF8(*Region);
        std::string AnsiService = Service;
        std::string AnsiVersion = Version;
        std::string Endpoint = AnsiService + ".tencentcloudapi.com";

        // 使用 TSharedPtr 管理所有对象
        TSharedPtr<TencentCloud::Credential> Credential = MakeShared<TencentCloud::Credential>(AnsiSecretId, AnsiSecretKey);
        TSharedPtr<TencentCloud::HttpProfile> HttpProfile = MakeShared<TencentCloud::HttpProfile>();
        HttpProfile->SetEndpoint(Endpoint);
        HttpProfile->SetReqTimeout(30);
        HttpProfile->SetConnectTimeout(30);

        TSharedPtr<TencentCloud::ClientProfile> ClientProfile = MakeShared<TencentCloud::ClientProfile>(*HttpProfile);

        // 创建 CommonClient - 直接使用值传递，不需要指针
        TencentCloud::CommonClient* RawClient = new TencentCloud::CommonClient(
            AnsiService,
            AnsiVersion,
            *Credential.Get(),  // 解引用 TSharedPtr
            AnsiRegion,
            *ClientProfile.Get()
        );

        // 将所有依赖对象都捕获到 lambda 中，确保它们和 Client 一起存活
        Client = TSharedPtr<TencentCloud::CommonClient>(
            RawClient,
            [Credential, HttpProfile, ClientProfile](TencentCloud::CommonClient* Ptr)
            {
                if (Ptr)
                {
                    UE_LOG(LogHunYuanAPI, Log, TEXT("Deleting CommonClient..."));

                    // 注意：这里不要立即删除，让 SDK 内部管理
                    // 直接 delete 可能会导致问题
                    delete Ptr;

                    UE_LOG(LogHunYuanAPI, Log, TEXT("CommonClient deleted"));
                }
                // Credential, HttpProfile, ClientProfile 会自动释放
            }
        );

        UE_LOG(LogHunYuanAPI, Log, TEXT("API client created successfully"));
        return true;
    }
    catch (const std::exception& e)
    {
        UE_LOG(LogHunYuanAPI, Error, TEXT("Failed to create client: %s"), UTF8_TO_TCHAR(e.what()));
        Client.Reset();
        return false;
    }
}

void FHunYuanAPIImpl::CancelAllRequests()
{
    // 清空请求队列
    FAPIRequest Request;
    while (RequestQueue.Dequeue(Request))
    {
        // 发送空结果
        AsyncTask(ENamedThreads::GameThread, [Callback = Request.Callback]()
            {
                Callback.ExecuteIfBound(nullptr);
            });
    }
}

void FHunYuanAPIImpl::CheckStatus() const
{
    UE_LOG(LogHunYuanAI, Log, TEXT("=== HunYuanAPI Status ==="));
    UE_LOG(LogHunYuanAI, Log, TEXT("bRunning: %d"), (bool)bRunning);
    UE_LOG(LogHunYuanAI, Log, TEXT("bIsValid: %d"), (bool)bIsValid);
    UE_LOG(LogHunYuanAI, Log, TEXT("WorkerThread: %s"), WorkerThread ? TEXT("valid") : TEXT("null"));
    UE_LOG(LogHunYuanAI, Log, TEXT("Client: %s"), Client.IsValid() ? TEXT("valid") : TEXT("null"));
    UE_LOG(LogHunYuanAI, Log, TEXT("RequestQueue is %s"),RequestQueue.IsEmpty() ? TEXT("empty") : TEXT("not empty"));
    UE_LOG(LogHunYuanAI, Log, TEXT("ResponseQueue is %s"),ResponseQueue.IsEmpty() ? TEXT("empty") : TEXT("not empty"));
    UE_LOG(LogHunYuanAI, Log, TEXT("========================="));
}