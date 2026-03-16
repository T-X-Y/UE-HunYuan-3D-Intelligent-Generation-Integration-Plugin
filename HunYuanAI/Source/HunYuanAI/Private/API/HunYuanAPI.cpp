#include "API/HunYuanAPI.h"
#include "API/HunYuanAPIImpl.h"
#include "HAL/PlatformProcess.h"

TSharedPtr<FHunYuanAPI> FHunYuanAPI::Instance = nullptr;
FCriticalSection FHunYuanAPI::InstanceCriticalSection;

TSharedPtr<FHunYuanAPI> FHunYuanAPI::Get()
{
    FScopeLock Lock(&InstanceCriticalSection);
    if (!Instance.IsValid())
    {
        Instance = MakeShareable(new FHunYuanAPI());
    }
    return Instance;
}

void FHunYuanAPI::Shutdown()
{
    FScopeLock Lock(&InstanceCriticalSection);
    if (Instance.IsValid())
    {
        Instance->CancelAllRequests();
        Instance.Reset();
    }
}

FHunYuanAPI::FHunYuanAPI()
    : bIsReady(false)
{
    Impl = MakeUnique<FHunYuanAPIImpl>();
}

FHunYuanAPI::~FHunYuanAPI()
{
    CancelAllRequests();
    Impl.Reset();
}

void FHunYuanAPI::SetCredentials(const FString& InSecretId, const FString& InSecretKey, const FString& InRegion)
{
    if (Impl.IsValid())
    {
        Impl->SetCredentials(InSecretId, InSecretKey, InRegion);
        bIsReady = true;
    }
}

void FHunYuanAPI::SubmitProJobFromText(const FString& Prompt, FOnJobSubmitted Callback)
{
    if (!Impl.IsValid() || !bIsReady)
    {
        Callback.ExecuteIfBound(false, TEXT("API not initialized"));
        return;
    }

    TSharedPtr<FJsonObject> Params = MakeShareable(new FJsonObject());
    Params->SetStringField(TEXT("Prompt"), Prompt);

    FOnAPIRequestComplete RequestCompleteDelegate = FOnAPIRequestComplete::CreateLambda(
        [Callback](const TSharedPtr<FJsonObject>& Result)
        {
            if (Result.IsValid())
            {
                FString JobId;
                if (Result->TryGetStringField(TEXT("JobId"), JobId))
                {
                    Callback.ExecuteIfBound(true, JobId);
                }
                else
                {
                    Callback.ExecuteIfBound(false, TEXT("Invalid response format"));
                }
            }
            else
            {
                Callback.ExecuteIfBound(false, TEXT("Request failed"));
            }
        });

    SendRequestAsync(TEXT("SubmitHunyuanTo3DProJob"), Params, RequestCompleteDelegate);
}

void FHunYuanAPI::SubmitProJobFromImage(const TArray<uint8>& ImageData, FOnJobSubmitted Callback)
{
    if (!Impl.IsValid() || !bIsReady)
    {
        Callback.ExecuteIfBound(false, TEXT("API not initialized"));
        return;
    }

    // TODO: 实现图生3D
    Callback.ExecuteIfBound(false, TEXT("Image to 3D not implemented yet"));
}

void FHunYuanAPI::QueryProJobResult(const FString& JobId, FOnJobQueried Callback)
{
    if (!Impl.IsValid() || !bIsReady)
    {
        Callback.ExecuteIfBound(false, nullptr);
        return;
    }

    TSharedPtr<FJsonObject> Params = MakeShareable(new FJsonObject());
    Params->SetStringField(TEXT("JobId"), JobId);

    FOnAPIRequestComplete RequestCompleteDelegate = FOnAPIRequestComplete::CreateLambda(
        [Callback](const TSharedPtr<FJsonObject>& Result)
        {
            Callback.ExecuteIfBound(Result.IsValid(), Result);
        });

    SendRequestAsync(TEXT("QueryHunyuanTo3DProJob"), Params, RequestCompleteDelegate);
}

void FHunYuanAPI::SendRequestAsync(const FString& Action, const TSharedPtr<FJsonObject>& Params,
    FOnAPIRequestComplete Callback)
{
    if (Impl.IsValid())
    {
        Impl->SendRequestAsync(Action, Params, Callback);
    }
    else
    {
        Callback.ExecuteIfBound(nullptr);
    }
}

void FHunYuanAPI::CancelAllRequests()
{
    if (Impl.IsValid())
    {
        Impl->CancelAllRequests();
    }
}

bool FHunYuanAPI::IsReady() const
{
    return bIsReady && Impl.IsValid() && Impl->IsValid();
}

void FHunYuanAPI::Tick()
{
    if (Impl.IsValid())
    {
        Impl->Tick();  // 转发给实现类
    }
}