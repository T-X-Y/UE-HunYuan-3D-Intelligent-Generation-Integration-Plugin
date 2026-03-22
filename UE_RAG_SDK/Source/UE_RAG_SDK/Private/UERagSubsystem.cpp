// UERagSubsystem.cpp
#include "UERagSubsystem.h"
#include "RagHttpClient.h"
#include "ResourceMonitor.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "Engine/GameInstance.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "Internationalization/Text.h"

DEFINE_LOG_CATEGORY_STATIC(LogUERag, Log, All);

void UUERagSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UE_LOG(LogUERag, Log, TEXT("RAG Subsystem 初始化"));

    // 创建HTTP客户端
    HttpClient = NewObject<URagHttpClient>(this);
    HttpClient->SetBaseUrl(ServiceUrl);

    // 创建资源监控器
    ResourceMonitor = NewObject<UResourceMonitor>(this);
    ResourceMonitor->Initialize(this);

    // 加载缓存（如果存在）
    LoadCache();
}

void UUERagSubsystem::Deinitialize()
{
    // 停止资源监控
    if (ResourceMonitor)
    {
        ResourceMonitor->StopMonitoring();
    }

    // 保存缓存
    SaveCache();

    // 清理
    HttpClient = nullptr;
    ResourceMonitor = nullptr;
    QueryCache.Empty();

    UE_LOG(LogUERag, Log, TEXT("RAG Subsystem 清理完成"));

    Super::Deinitialize();
}

void UUERagSubsystem::SetServiceUrl(const FString& Url)
{
    ServiceUrl = Url;
    if (HttpClient)
    {
        HttpClient->SetBaseUrl(Url);
    }
    UE_LOG(LogUERag, Log, TEXT("RAG服务URL已设置为: %s"), *Url);
}

void UUERagSubsystem::QueryRagAsync(const FQueryRequest& Request, FOnQueryCompleteDelegate InDelegate)
{
    // 检查缓存
    FString CacheKey = GetCacheKey(Request);
    if (QueryCache.Contains(CacheKey))
    {
        FQueryResponse CachedResponse = QueryCache[CacheKey];
        InDelegate.ExecuteIfBound(CachedResponse);
        OnQueryCompleted.Broadcast(CachedResponse);  // 添加蓝图广播
        return;
    }

    // 创建HTTP请求
    TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject);
    RequestJson->SetStringField(TEXT("query"), Request.Query);
    RequestJson->SetNumberField(TEXT("top_k"), Request.TopK);
    if (!Request.FilterType.IsEmpty())
    {
        RequestJson->SetStringField(TEXT("filter_type"), Request.FilterType);
    }

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

    // 发送请求，使用正确的回调
    HttpClient->PostJson(TEXT("/query"), RequestBody,
        FOnHttpRequestComplete::CreateUObject(this, &UUERagSubsystem::HandleQueryResponse, CacheKey, InDelegate));
}

void UUERagSubsystem::ImportResource(const FResourceMetadata& Metadata, UObject* ResourceAsset)
{
    TSharedPtr<FJsonObject> MetadataJson = MakeShareable(new FJsonObject);
    MetadataJson->SetStringField(TEXT("resource_id"), Metadata.ResourceId);
    MetadataJson->SetStringField(TEXT("resource_name"), Metadata.ResourceName);
    MetadataJson->SetStringField(TEXT("resource_type"), Metadata.ResourceType);
    MetadataJson->SetStringField(TEXT("resource_path"), Metadata.ResourcePath);
    MetadataJson->SetStringField(TEXT("description"), Metadata.Description);

    // 处理标签
    TArray<TSharedPtr<FJsonValue>> TagsArray;
    for (const FString& Tag : Metadata.Tags)
    {
        TagsArray.Add(MakeShareable(new FJsonValueString(Tag)));
    }
    MetadataJson->SetArrayField(TEXT("tags"), TagsArray);

    // 处理额外数据
    if (!Metadata.AdditionalDataJson.IsEmpty())
    {
        TSharedPtr<FJsonObject> AdditionalData;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Metadata.AdditionalDataJson);
        if (FJsonSerializer::Deserialize(Reader, AdditionalData))
        {
            MetadataJson->SetObjectField(TEXT("additional_data"), AdditionalData);
        }
    }

    // 序列化元数据
    FString MetadataString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MetadataString);
    FJsonSerializer::Serialize(MetadataJson.ToSharedRef(), Writer);

    // 检查是否有文件需要上传
    if (ResourceAsset)
    {
        // 导出资源到临时文件
        FString TempFilePath = FPaths::ProjectSavedDir() / TEXT("RAGTemp") / FGuid::NewGuid().ToString() + TEXT(".json");

        // 这里需要根据资源类型实现导出逻辑
        // 简单起见，我们只导出资源信息
        bool bExported = ExportAssetInfo(ResourceAsset, TempFilePath);

        if (bExported && FPaths::FileExists(TempFilePath))
        {
            // 上传文件
            TMap<FString, FString> FormFields;
            FormFields.Add(TEXT("metadata"), MetadataString);

            HttpClient->PostFile(TEXT("/resources/import"), TempFilePath, TEXT("file"), FormFields,
                FOnHttpRequestComplete::CreateUObject(this, &UUERagSubsystem::HandleImportResponse));
        }
    }
    else
    {
        // 只有元数据，没有文件
        TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject);
        RequestJson->SetObjectField(TEXT("metadata"), MetadataJson);

        FString RequestBody;
        TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&RequestBody);
        FJsonSerializer::Serialize(RequestJson.ToSharedRef(), JsonWriter);

        HttpClient->PostJson(TEXT("/resources/import"), RequestBody,
            FOnHttpRequestComplete::CreateUObject(this, &UUERagSubsystem::HandleImportResponse));
    }
}

void UUERagSubsystem::ImportResources(const TArray<FResourceMetadata>& MetadataList)
{
    for (const FResourceMetadata& Metadata : MetadataList)
    {
        ImportResource(Metadata, nullptr);
    }
}

void UUERagSubsystem::StartResourceMonitoring()
{
    if (ResourceMonitor)
    {
        ResourceMonitor->StartMonitoring();
        UE_LOG(LogUERag, Log, TEXT("资源监控已启动"));
    }
}

void UUERagSubsystem::NotifyResourceChanged(EResourceEventType EventType, const FResourceMetadata& Resource)
{
    // 构建更新事件
    TSharedPtr<FJsonObject> EventJson = MakeShareable(new FJsonObject);

    // 事件类型转换
    FString EventTypeStr;
    switch (EventType)
    {
    case EResourceEventType::Add:    EventTypeStr = TEXT("add"); break;
    case EResourceEventType::Update: EventTypeStr = TEXT("update"); break;
    case EResourceEventType::Delete: EventTypeStr = TEXT("delete"); break;
    }
    EventJson->SetStringField(TEXT("event_type"), EventTypeStr);

    // 资源元数据
    TSharedPtr<FJsonObject> ResourceJson = MakeShareable(new FJsonObject);
    ResourceJson->SetStringField(TEXT("resource_id"), Resource.ResourceId);
    ResourceJson->SetStringField(TEXT("resource_name"), Resource.ResourceName);
    ResourceJson->SetStringField(TEXT("resource_type"), Resource.ResourceType);
    ResourceJson->SetStringField(TEXT("resource_path"), Resource.ResourcePath);
    ResourceJson->SetStringField(TEXT("description"), Resource.Description);

    // 标签
    TArray<TSharedPtr<FJsonValue>> TagsArray;
    for (const FString& Tag : Resource.Tags)
    {
        TagsArray.Add(MakeShareable(new FJsonValueString(Tag)));
    }
    ResourceJson->SetArrayField(TEXT("tags"), TagsArray);

    EventJson->SetObjectField(TEXT("resource"), ResourceJson);
    EventJson->SetStringField(TEXT("timestamp"), FDateTime::Now().ToString());

    // 发送更新
    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(EventJson.ToSharedRef(), Writer);

    HttpClient->PostJson(TEXT("/update"), RequestBody,
        FOnHttpRequestComplete::CreateUObject(this, &UUERagSubsystem::HandleUpdateResponse));

    // 蓝图事件广播
    switch (EventType)
    {
    case EResourceEventType::Add:
        OnResourceAdded.Broadcast(Resource);
        break;
    case EResourceEventType::Update:
        OnResourceUpdated.Broadcast(Resource);
        break;
    case EResourceEventType::Delete:
        OnResourceDeleted.Broadcast(Resource);
        break;
    }
}

void UUERagSubsystem::HandleQueryResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully,
    FString CacheKey, FOnQueryCompleteDelegate InDelegate)
{
    FQueryResponse QueryResponse;

    if (bConnectedSuccessfully && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
    {
        FString ResponseStr = Response->GetContentAsString();
        TSharedPtr<FJsonObject> ResponseJson;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);

        if (FJsonSerializer::Deserialize(Reader, ResponseJson) && ResponseJson.IsValid())
        {
            QueryResponse = ParseQueryResponse(ResponseJson);
            QueryResponse.bSuccess = true;

            // 缓存结果
            QueryCache.Add(CacheKey, QueryResponse);
        }
        else
        {
            QueryResponse.bSuccess = false;
            QueryResponse.ErrorMessage = TEXT("解析响应失败");
        }
    }
    else
    {
        QueryResponse.bSuccess = false;
        QueryResponse.ErrorMessage = FString::Printf(TEXT("HTTP请求失败: %d"),
            Response.IsValid() ? Response->GetResponseCode() : -1);
    }

    // 回调
    InDelegate.ExecuteIfBound(QueryResponse);
    OnQueryCompleted.Broadcast(QueryResponse);
}

FQueryResponse UUERagSubsystem::ParseQueryResponse(TSharedPtr<FJsonObject> JsonObj)
{
    FQueryResponse Response;

    if (JsonObj->HasField(TEXT("answer")))
    {
        Response.Answer = JsonObj->GetStringField(TEXT("answer"));
    }

    if (JsonObj->HasField(TEXT("sources")))
    {
        TArray<TSharedPtr<FJsonValue>> SourcesArray = JsonObj->GetArrayField(TEXT("sources"));
        for (auto& SourceValue : SourcesArray)
        {
            TSharedPtr<FJsonObject> SourceObj = SourceValue->AsObject();
            if (SourceObj.IsValid())
            {
                FSourceDocument Source;

                if (SourceObj->HasField(TEXT("content")))
                    Source.Content = SourceObj->GetStringField(TEXT("content"));

                if (SourceObj->HasField(TEXT("metadata")))
                {
                    TSharedPtr<FJsonObject> MetaObj = SourceObj->GetObjectField(TEXT("metadata"));
                    if (MetaObj.IsValid())
                    {
                        if (MetaObj->HasField(TEXT("resource_id")))
                            Source.SourceId = MetaObj->GetStringField(TEXT("resource_id"));

                        if (MetaObj->HasField(TEXT("resource_name")))
                            Source.ResourceName = MetaObj->GetStringField(TEXT("resource_name"));

                        if (MetaObj->HasField(TEXT("resource_type")))
                            Source.ResourceType = MetaObj->GetStringField(TEXT("resource_type"));
                    }
                }

                Response.Sources.Add(Source);
            }
        }
    }

    if (JsonObj->HasField(TEXT("processing_time")))
    {
        Response.ProcessingTime = JsonObj->GetNumberField(TEXT("processing_time"));
    }

    return Response;
}

void UUERagSubsystem::HandleImportResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
    if (bConnectedSuccessfully && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
    {
        UE_LOG(LogUERag, Log, TEXT("资源导入成功: %s"), *Response->GetContentAsString());
    }
    else
    {
        UE_LOG(LogUERag, Warning, TEXT("资源导入失败"));
    }
}

void UUERagSubsystem::HandleUpdateResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
    if (bConnectedSuccessfully && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
    {
        UE_LOG(LogUERag, Log, TEXT("更新事件发送成功: %s"), *Response->GetContentAsString());
    }
    else
    {
        UE_LOG(LogUERag, Warning, TEXT("更新事件发送失败"));
    }
}

FString UUERagSubsystem::GetCacheKey(const FQueryRequest& Request)
{
    return FString::Printf(TEXT("%s_%d_%s"),
        *Request.Query,
        Request.TopK,
        *Request.FilterType);
}

void UUERagSubsystem::LoadCache()
{
    FString CacheFile = FPaths::ProjectSavedDir() / TEXT("RAGCache.json");

    if (FPaths::FileExists(CacheFile))
    {
        FString JsonStr;
        if (FFileHelper::LoadFileToString(JsonStr, *CacheFile))
        {
            TSharedPtr<FJsonObject> Root;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);

            if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
            {
                for (auto& Pair : Root->Values)
                {
                    TSharedPtr<FJsonObject> ResponseObj = Pair.Value->AsObject();
                    if (ResponseObj.IsValid())
                    {
                        FQueryResponse Response;
                        Response.Answer = ResponseObj->GetStringField(TEXT("answer"));

                        // 解析sources...
                        // 这里简化处理，实际应该完整解析

                        QueryCache.Add(Pair.Key, Response);
                    }
                }
            }
        }
    }
}

void UUERagSubsystem::SaveCache()
{
    if (QueryCache.Num() == 0) return;

    TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject);

    for (auto& Pair : QueryCache)
    {
        TSharedPtr<FJsonObject> ResponseObj = MakeShareable(new FJsonObject);
        ResponseObj->SetStringField(TEXT("answer"), Pair.Value.Answer);

        // 保存sources...（简化）

        Root->SetObjectField(Pair.Key, ResponseObj);
    }

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    FString CacheFile = FPaths::ProjectSavedDir() / TEXT("RAGCache.json");
    FFileHelper::SaveStringToFile(JsonStr, *CacheFile);
}

bool UUERagSubsystem::ExportAssetInfo(UObject* Asset, const FString& OutputPath)
{
    if (!Asset) return false;

    TSharedPtr<FJsonObject> AssetInfo = MakeShareable(new FJsonObject);
    AssetInfo->SetStringField(TEXT("class"), Asset->GetClass()->GetName());
    AssetInfo->SetStringField(TEXT("name"), Asset->GetName());
    AssetInfo->SetStringField(TEXT("path"), Asset->GetPathName());

    // 获取资源属性（可以根据需要扩展）
    // 这里可以添加更多属性提取逻辑

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    FJsonSerializer::Serialize(AssetInfo.ToSharedRef(), Writer);

    return FFileHelper::SaveStringToFile(JsonStr, *OutputPath);
}