// RagHttpClient.cpp
#include "RagHttpClient.h"
#include "HttpModule.h"
#include "HttpManager.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Object.h"

DEFINE_LOG_CATEGORY_STATIC(LogRagHttpClient, Log, All);

void URagHttpClient::PostJson(const FString& Endpoint, const FString& JsonContent, FOnHttpRequestComplete CompleteDelegate)
{
    // 创建HTTP请求
    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule.CreateRequest();

    // 构建完整URL
    FString Url = BaseUrl + Endpoint;

    // 设置请求参数
    Request->SetURL(Url);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Request->SetContentAsString(JsonContent);

    UE_LOG(LogRagHttpClient, Verbose, TEXT("发送POST请求到: %s"), *Url);
    UE_LOG(LogRagHttpClient, VeryVerbose, TEXT("请求体: %s"), *JsonContent);

    // 绑定回调
    Request->OnProcessRequestComplete().BindUObject(this, &URagHttpClient::OnRequestComplete, CompleteDelegate);

    // 发送请求
    Request->ProcessRequest();
}

bool URagHttpClient::PostJsonSync(const FString& Endpoint, const FString& JsonContent, TSharedPtr<FJsonObject>& OutResponse)
{
    FString Url = BaseUrl + Endpoint;

    // 创建HTTP请求
    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule.CreateRequest();

    Request->SetURL(Url);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetContentAsString(JsonContent);

    // 同步请求需要使用FHttpModule的同步请求功能
    // 注意：这可能会阻塞游戏线程，谨慎使用
    bool bProcessed = Request->ProcessRequest();

    if (!bProcessed)
    {
        UE_LOG(LogRagHttpClient, Warning, TEXT("同步请求处理失败: %s"), *Url);
        return false;
    }

    // 等待请求完成（简单实现，实际可能需要更复杂的等待机制）
    float Timeout = 10.0f; // 10秒超时
    float Elapsed = 0.0f;
    float TickRate = 0.1f;

    while (Request->GetStatus() <= EHttpRequestStatus::Processing && Elapsed < Timeout)
    {
        FPlatformProcess::Sleep(TickRate);
        Elapsed += TickRate;

        // 让引擎处理网络事件
        FHttpModule::Get().GetHttpManager().Tick(Elapsed);
    }

    if (Request->GetStatus() != EHttpRequestStatus::Succeeded)
    {
        UE_LOG(LogRagHttpClient, Warning, TEXT("同步请求超时或失败: %s, 状态: %d"),
            *Url, static_cast<int32>(Request->GetStatus()));
        return false;
    }

    // 获取响应
    FHttpResponsePtr Response = Request->GetResponse();
    if (!Response.IsValid() || !EHttpResponseCodes::IsOk(Response->GetResponseCode()))
    {
        UE_LOG(LogRagHttpClient, Warning, TEXT("同步请求响应无效: %d"),
            Response.IsValid() ? Response->GetResponseCode() : -1);
        return false;
    }

    // 解析JSON响应
    FString ResponseStr = Response->GetContentAsString();
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);

    if (!FJsonSerializer::Deserialize(Reader, OutResponse) || !OutResponse.IsValid())
    {
        UE_LOG(LogRagHttpClient, Warning, TEXT("同步请求响应解析失败"));
        return false;
    }

    return true;
}

void URagHttpClient::PostFile(const FString& Endpoint, const FString& FilePath, const FString& FileFieldName,
    const TMap<FString, FString>& FormFields, FOnHttpRequestComplete CompleteDelegate)
{
    // 检查文件是否存在
    if (!FPaths::FileExists(FilePath))
    {
        UE_LOG(LogRagHttpClient, Error, TEXT("文件不存在: %s"), *FilePath);
        return;
    }

    // 读取文件内容
    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *FilePath))
    {
        UE_LOG(LogRagHttpClient, Error, TEXT("无法读取文件: %s"), *FilePath);
        return;
    }

    // 创建multipart/form-data请求
    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule.CreateRequest();

    FString Url = BaseUrl + Endpoint;
    Request->SetURL(Url);
    Request->SetVerb(TEXT("POST"));

    // 生成boundary
    FString Boundary = FString::Printf(TEXT("---------------------------%s"), *FGuid::NewGuid().ToString().Replace(TEXT("-"), TEXT("")));
    FString ContentTypeHeader = FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary);
    Request->SetHeader(TEXT("Content-Type"), *ContentTypeHeader);

    // 构建multipart数据
    TArray<uint8> RequestData;
    FString BoundaryDelimiter = TEXT("--") + Boundary + TEXT("\r\n");

    // 添加表单字段
    for (const auto& Field : FormFields)
    {
        // 添加字段头
        RequestData.Append((uint8*)TCHAR_TO_UTF8(*BoundaryDelimiter), BoundaryDelimiter.Len());

        FString FieldHeader = FString::Printf(TEXT("Content-Disposition: form-data; name=\"%s\"\r\n\r\n"), *Field.Key);
        RequestData.Append((uint8*)TCHAR_TO_UTF8(*FieldHeader), FieldHeader.Len());

        // 添加字段值
        FString FieldValue = Field.Value + TEXT("\r\n");
        RequestData.Append((uint8*)TCHAR_TO_UTF8(*FieldValue), FieldValue.Len());
    }

    // 添加文件字段
    RequestData.Append((uint8*)TCHAR_TO_UTF8(*BoundaryDelimiter), BoundaryDelimiter.Len());

    FString FileFieldHeader = FString::Printf(
        TEXT("Content-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n"),
        *FileFieldName,
        *FPaths::GetCleanFilename(FilePath)
    );
    RequestData.Append((uint8*)TCHAR_TO_UTF8(*FileFieldHeader), FileFieldHeader.Len());

    FString FileContentType = TEXT("Content-Type: application/octet-stream\r\n\r\n");
    RequestData.Append((uint8*)TCHAR_TO_UTF8(*FileContentType), FileContentType.Len());

    // 添加文件数据
    RequestData.Append(FileData);

    // 添加文件结束标记
    FString FileEnding = TEXT("\r\n");
    RequestData.Append((uint8*)TCHAR_TO_UTF8(*FileEnding), FileEnding.Len());

    // 添加结束boundary
    FString EndBoundary = TEXT("--") + Boundary + TEXT("--\r\n");
    RequestData.Append((uint8*)TCHAR_TO_UTF8(*EndBoundary), EndBoundary.Len());

    // 设置请求内容
    Request->SetContent(RequestData);

    UE_LOG(LogRagHttpClient, Log, TEXT("上传文件到: %s, 文件: %s, 大小: %d 字节"),
        *Url, *FPaths::GetCleanFilename(FilePath), FileData.Num());

    // 绑定回调
    Request->OnProcessRequestComplete().BindUObject(this, &URagHttpClient::OnRequestComplete, CompleteDelegate);

    // 发送请求
    Request->ProcessRequest();
}

void URagHttpClient::OnRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully,
    FOnHttpRequestComplete CompleteDelegate)
{
    // 记录请求结果
    if (bConnectedSuccessfully && Response.IsValid())
    {
        int32 ResponseCode = Response->GetResponseCode();
        if (EHttpResponseCodes::IsOk(ResponseCode))
        {
            UE_LOG(LogRagHttpClient, Verbose, TEXT("请求成功: %s, 响应码: %d"),
                *Request->GetURL(), ResponseCode);
            UE_LOG(LogRagHttpClient, VeryVerbose, TEXT("响应体: %s"), *Response->GetContentAsString());
        }
        else
        {
            UE_LOG(LogRagHttpClient, Warning, TEXT("请求失败: %s, 响应码: %d, 错误: %s"),
                *Request->GetURL(), ResponseCode, *Response->GetContentAsString());
        }
    }
    else
    {
        UE_LOG(LogRagHttpClient, Error, TEXT("请求连接失败: %s"), *Request->GetURL());
    }

    // 调用委托
    CompleteDelegate.ExecuteIfBound(Request, Response, bConnectedSuccessfully);
}