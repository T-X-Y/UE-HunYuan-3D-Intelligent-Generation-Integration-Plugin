#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "RagTypes.generated.h"

// 前向声明
struct FQueryResponse;



// UE资源元数据（与Python服务对应）
USTRUCT(BlueprintType)
struct FResourceMetadata
{
    GENERATED_BODY()

    // 唯一ID（可用UE的Guid或路径哈希）
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString ResourceId;

    // 资源名称
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString ResourceName;

    // 资源类型（mesh/texture/blueprint/document等）
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString ResourceType;

    // UE内部路径
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString ResourcePath;

    // 标签
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    TArray<FString> Tags;

    // 描述
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString Description;

    // 额外数据（JSON字符串）
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString AdditionalDataJson;
};

// 查询请求
USTRUCT(BlueprintType)
struct FQueryRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString Query;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    int32 TopK = 5;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString FilterType;
};

// 来源文档
USTRUCT(BlueprintType)
struct FSourceDocument
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString Content;

    UPROPERTY(BlueprintReadOnly)
    FString SourceId;

    UPROPERTY(BlueprintReadOnly)
    FString ResourceName;

    UPROPERTY(BlueprintReadOnly)
    FString ResourceType;
};

// 查询响应
USTRUCT(BlueprintType)
struct FQueryResponse
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString Answer;

    UPROPERTY(BlueprintReadOnly)
    TArray<FSourceDocument> Sources;

    UPROPERTY(BlueprintReadOnly)
    float ProcessingTime = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    bool bSuccess = false;

    UPROPERTY(BlueprintReadOnly)
    FString ErrorMessage;
};

// 资源更新事件类型
UENUM(BlueprintType)
enum class EResourceEventType : uint8
{
    Add,
    Update,
    Delete
};

// 资源更新事件（用于蓝图广播）
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResourceUpdated, const FResourceMetadata&, Resource);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQueryCompleted, const FQueryResponse&, Response);

// 委托用于C++回调
DECLARE_DELEGATE_OneParam(FOnQueryCompleteDelegate, const FQueryResponse&);