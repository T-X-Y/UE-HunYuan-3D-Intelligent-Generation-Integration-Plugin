#pragma once

#include "CoreMinimal.h"
#include "Misc/DateTime.h"
#include "Dom/JsonObject.h"

// 历史记录类型
enum class EHistoryEntryType : uint8
{
    Generation,
    Import,
    Download,
    API,
    Config,
    Error
};

// 任务状态
enum class EHistoryTaskStatus : uint8
{
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled
};

// 基础历史记录条目 - 前向声明
struct FDownloadHistoryEntry;
struct FAPIHistoryEntry;

// 基础历史记录条目
struct FHistoryEntry
{
    // 基本信息
    FString EntryId;
    EHistoryEntryType Type;
    FDateTime Timestamp;
    FString Description;

    // 状态信息
    EHistoryTaskStatus Status;
    float Progress;

    // 关联信息
    FString JobId;
    FString UserName;
    FString SessionId;

    // 性能数据
    double Duration = 0.0;
    int64 MemoryUsage = 0;

    // 元数据
    TSharedPtr<FJsonObject> Metadata;

    FHistoryEntry()
        : EntryId(FGuid::NewGuid().ToString())
        , Timestamp(FDateTime::Now())
        , Status(EHistoryTaskStatus::Pending)
        , Progress(0.0f)
        , Duration(0.0)
        , MemoryUsage(0)
    {
    }

    virtual ~FHistoryEntry() {}

    virtual TSharedPtr<FJsonObject> ToJson() const;
    virtual bool FromJson(const TSharedPtr<FJsonObject>& Json);
};

// 下载历史记录
struct FDownloadHistoryEntry : public FHistoryEntry
{
    FString URL;
    FString LocalPath;
    int64 FileSize = 0;
    int64 DownloadedSize = 0;
    float DownloadSpeed = 0.0f;
    int32 RetryCount = 0;
    FString FileHash;

    FDownloadHistoryEntry()
    {
        Type = EHistoryEntryType::Download;
    }

    virtual TSharedPtr<FJsonObject> ToJson() const override;
    virtual bool FromJson(const TSharedPtr<FJsonObject>& Json) override;
};

// API调用历史记录
struct FAPIHistoryEntry : public FHistoryEntry
{
    FString Action;
    TSharedPtr<FJsonObject> Request;
    TSharedPtr<FJsonObject> Response;
    int32 HttpCode = 0;
    FString Endpoint;
    double RequestTime = 0.0;

    FAPIHistoryEntry()
    {
        Type = EHistoryEntryType::API;
    }

    virtual TSharedPtr<FJsonObject> ToJson() const override;
    virtual bool FromJson(const TSharedPtr<FJsonObject>& Json) override;
};

// 统计信息
struct FHistoryStatistics
{
    int32 TotalEntries = 0;
    int32 GenerationCount = 0;
    int32 ImportCount = 0;
    int32 DownloadCount = 0;
    int32 APICount = 0;
    int32 ConfigCount = 0;
    int32 ErrorCount = 0;

    int32 SuccessCount = 0;
    int32 FailedCount = 0;
    int32 PendingCount = 0;

    double TotalDuration = 0.0;
    double AverageDuration = 0.0;

    FDateTime FirstEntry;
    FDateTime LastEntry;

    TMap<FString, int32> DailyStats;
};