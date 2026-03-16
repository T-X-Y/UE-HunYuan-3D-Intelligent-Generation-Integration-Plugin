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
    , WorkerThread(nullptr)
    , Client(nullptr)
    , CredentialPtr(nullptr)
    , HttpProfilePtr(nullptr)
    , ClientProfilePtr(nullptr)
{
    Service = "ai3d";
    Version = "2025-05-13";
    TencentCloud::InitAPI();
}

FHunYuanAPIImpl::~FHunYuanAPIImpl()
{
    UE_LOG(LogHunYuanAPI, Log, TEXT("FHunYuanAPIImpl destructor started"));

    // 标记线程停止
    Stop();

    // 等待线程结束
    if (WorkerThread)
    {
        WorkerThread->WaitForCompletion();
        delete WorkerThread;
        WorkerThread = nullptr;
    }

    // 清理客户端
    {
        FScopeLock Lock(&ClientCriticalSection);
        Client.Reset();
    }

    // ===== 清理成员变量（TSharedPtr 会自动处理，但显式 Reset 更清晰）=====
    CredentialPtr.Reset();
    HttpProfilePtr.Reset();
    ClientProfilePtr.Reset();

    TencentCloud::ShutdownAPI();

    UE_LOG(LogHunYuanAPI, Log, TEXT("FHunYuanAPIImpl destructor completed"));
}

void FHunYuanAPIImpl::SetCredentials(const FString& InSecretId, const FString& InSecretKey, const FString& InRegion)
{
    FScopeLock Lock(&ClientCriticalSection);

    SecretId = InSecretId;
    SecretKey = InSecretKey;
    Region = InRegion.IsEmpty() ? TEXT("ap-guangzhou") : InRegion;

    // 重置客户端，下次请求时会重新创建
    Client.Reset();

    // ===== 同时也重置 SDK 对象 =====
    CredentialPtr.Reset();
    HttpProfilePtr.Reset();
    ClientProfilePtr.Reset();

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

    if (!CreateClient())
    {
        UE_LOG(LogHunYuanAI, Error, TEXT("[%s] Failed to create API client"), *Request.RequestId);
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
            }
        }
        else
        {
            Core::Error error = httpOutcome.GetError();
            UE_LOG(LogHunYuanAI, Error, TEXT("[%s] Request failed: %s"),
                *Request.RequestId, UTF8_TO_TCHAR(error.GetErrorMessage().c_str()));
        }
    }
    catch (const std::exception& e)
    {
        UE_LOG(LogHunYuanAI, Error, TEXT("[%s] Exception in API request: %s"),
            *Request.RequestId, UTF8_TO_TCHAR(e.what()));
    }
    catch (...)
    {
        UE_LOG(LogHunYuanAI, Error, TEXT("[%s] Unknown exception in API request"), *Request.RequestId);
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

        // ===== 使用 UE 的 MakeShareable，并修正命名空间 =====
        CredentialPtr = MakeShareable(new TencentCloud::Credential(AnsiSecretId, AnsiSecretKey));

        // 注意：去掉 Profile::，直接在 TencentCloud 命名空间下
        HttpProfilePtr = MakeShareable(new TencentCloud::HttpProfile());
        HttpProfilePtr->SetEndpoint(Endpoint);
        HttpProfilePtr->SetReqTimeout(30);
        HttpProfilePtr->SetConnectTimeout(30);

        // 注意：去掉 Profile::，直接在 TencentCloud 命名空间下
        ClientProfilePtr = MakeShareable(new TencentCloud::ClientProfile(*HttpProfilePtr));

        // 创建 CommonClient
        TencentCloud::CommonClient* RawClient = new TencentCloud::CommonClient(
            AnsiService,
            AnsiVersion,
            *CredentialPtr,
            AnsiRegion,
            *ClientProfilePtr
        );

        Client = MakeShareable(RawClient);

        UE_LOG(LogHunYuanAPI, Log, TEXT("API client created successfully"));
        return true;
    }
    catch (const std::exception& e)
    {
        UE_LOG(LogHunYuanAPI, Error, TEXT("Failed to create client: %s"), UTF8_TO_TCHAR(e.what()));

        // UE的TSharedPtr用Reset()
        CredentialPtr.Reset();
        HttpProfilePtr.Reset();
        ClientProfilePtr.Reset();

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