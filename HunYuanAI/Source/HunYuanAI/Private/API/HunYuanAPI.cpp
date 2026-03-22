// API/HunYuanAPI.cpp

#include "API/HunYuanAPI.h"
#include "API/HunYuanAPIImpl.h"
#include "HAL/PlatformProcess.h"
#include "Logging/HunYuanLogging.h"

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

void FHunYuanAPI::SubmitProJobFromText(const FString& Prompt, HunYuanAPI::EModelFormat Format, FOnJobSubmitted Callback)
{
    if (!Impl.IsValid() || !bIsReady)
    {
        Callback.ExecuteIfBound(false, TEXT("API not initialized"));
        return;
    }

    TSharedPtr<FJsonObject> Params = MakeShareable(new FJsonObject());
    Params->SetStringField(TEXT("Prompt"), Prompt);
    Params->SetStringField(TEXT("GenerateType"), TEXT("LowPoly"));

    FString FormatStr = GetFormatString(Format);
    if (!FormatStr.IsEmpty())
    {
        Params->SetStringField(TEXT("ResultFormat"), FormatStr);
        UE_LOG(LogTemp, Log, TEXT("设置ResultFormat为: %s"), *FormatStr);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("使用默认格式: OBJ+GLB"));
    }

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

void FHunYuanAPI::SubmitProJobFromImage(const TArray<uint8>& ImageData, HunYuanAPI::EModelFormat Format, FOnJobSubmitted Callback)
{
    if (!Impl.IsValid() || !bIsReady)
    {
        Callback.ExecuteIfBound(false, TEXT("API not initialized"));
        return;
    }

    // 验证图片数据
    if (ImageData.Num() == 0)
    {
        Callback.ExecuteIfBound(false, TEXT("Image data is empty"));
        return;
    }

    if (ImageData.Num() > 6 * 1024 * 1024)  // 6MB限制
    {
        Callback.ExecuteIfBound(false, TEXT("Image data exceeds 6MB limit"));
        return;
    }

    // Base64编码
    FString Base64Image = FBase64::Encode(ImageData);

    TSharedPtr<FJsonObject> Params = MakeShareable(new FJsonObject());

    // 使用正确的字段名 - 根据API定义
    Params->SetStringField(TEXT("ImageBase64"), Base64Image);

    // 设置生成参数
    Params->SetStringField(TEXT("GenerateType"), TEXT("Normal"));  // 或根据需求
    Params->SetBoolField(TEXT("EnablePBR"), true);
    Params->SetNumberField(TEXT("FaceCount"), static_cast<double>(500000));
    Params->SetStringField(TEXT("Model"), TEXT("3.1"));  // 使用最新版本

    // 设置输出格式
    FString FormatStr = GetFormatString(Format);
    if (!FormatStr.IsEmpty())
    {
        Params->SetStringField(TEXT("ResultFormat"), FormatStr);
    }

    // 使用弱引用来避免悬空指针
    TWeakPtr<FHunYuanAPI> WeakAPI = AsShared();

    FOnAPIRequestComplete RequestCompleteDelegate = FOnAPIRequestComplete::CreateLambda(
        [WeakAPI, Callback](const TSharedPtr<FJsonObject>& Result)
        {
            // 检查对象是否还存在
            if (!WeakAPI.IsValid())
            {
                UE_LOG(LogHunYuanAPI, Warning, TEXT("FHunYuanAPI destroyed before callback"));
                return;
            }

            // 确保回调在游戏线程执行
            if (!IsInGameThread())
            {
                // 如果在非游戏线程，调度到游戏线程
                AsyncTask(ENamedThreads::GameThread, [WeakAPI, Callback, Result]()
                    {
                        if (!WeakAPI.IsValid())
                        {
                            return;
                        }

                        if (!Result.IsValid())
                        {
                            Callback.ExecuteIfBound(false, TEXT("Request failed - no response"));
                            return;
                        }

                        // 检查是否有错误
                        FString ErrorCode, ErrorMessage;
                        if (Result->TryGetStringField(TEXT("ErrorCode"), ErrorCode))
                        {
                            Result->TryGetStringField(TEXT("ErrorMessage"), ErrorMessage);
                            Callback.ExecuteIfBound(false, FString::Printf(TEXT("%s: %s"), *ErrorCode, *ErrorMessage));
                            return;
                        }

                        // 获取 JobId
                        FString JobId;
                        if (Result->TryGetStringField(TEXT("JobId"), JobId))
                        {
                            Callback.ExecuteIfBound(true, JobId);
                        }
                        else
                        {
                            Callback.ExecuteIfBound(false, TEXT("Invalid response: missing JobId"));
                        }
                    });
                return;
            }

            // 已经在游戏线程，直接处理
            if (!Result.IsValid())
            {
                Callback.ExecuteIfBound(false, TEXT("Request failed - no response"));
                return;
            }

            // 检查是否有错误
            FString ErrorCode, ErrorMessage;
            if (Result->TryGetStringField(TEXT("ErrorCode"), ErrorCode))
            {
                Result->TryGetStringField(TEXT("ErrorMessage"), ErrorMessage);
                Callback.ExecuteIfBound(false, FString::Printf(TEXT("%s: %s"), *ErrorCode, *ErrorMessage));
                return;
            }

            // 获取 JobId
            FString JobId;
            if (Result->TryGetStringField(TEXT("JobId"), JobId))
            {
                Callback.ExecuteIfBound(true, JobId);
            }
            else
            {
                Callback.ExecuteIfBound(false, TEXT("Invalid response: missing JobId"));
            }
        });

    SendRequestAsync(TEXT("SubmitHunyuanTo3DProJob"), Params, RequestCompleteDelegate);
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