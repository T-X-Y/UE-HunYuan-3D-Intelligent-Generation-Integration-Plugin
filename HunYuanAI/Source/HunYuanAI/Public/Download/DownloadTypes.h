#pragma once

#include "CoreMinimal.h"

namespace Download
{
    // 下载状态
    enum class EDownloadStatus
    {
        NotStarted,
        Downloading,
        Paused,
        Completed,
        Cancelled,
        Failed
    };

    // 下载优先级
    enum class EPriority
    {
        Low,
        Normal,
        High,
        Critical
    };

    // 下载项信息
    struct FDownloadItem
    {
        FString URL;
        FString JobId;
        FString DestinationPath;
        FString FileName;
        int64 TotalBytes = 0;
        int64 ReceivedBytes = 0;
        float Progress = 0.0f;
        EDownloadStatus Status = EDownloadStatus::NotStarted;
        FString ErrorMessage;

        // 计算进度百分比
        float GetProgressPercent() const
        {
            return TotalBytes > 0 ? (float)ReceivedBytes / TotalBytes : 0.0f;
        }

        // 获取下载速度字符串
        FString GetSpeedString(int64 BytesPerSecond) const
        {
            if (BytesPerSecond < 1024)
                return FString::Printf(TEXT("%lld B/s"), BytesPerSecond);
            else if (BytesPerSecond < 1024 * 1024)
                return FString::Printf(TEXT("%.1f KB/s"), BytesPerSecond / 1024.0f);
            else
                return FString::Printf(TEXT("%.1f MB/s"), BytesPerSecond / (1024.0f * 1024.0f));
        }

        // 获取已下载大小字符串
        FString GetReceivedSizeString() const
        {
            if (ReceivedBytes < 1024)
                return FString::Printf(TEXT("%lld B"), ReceivedBytes);
            else if (ReceivedBytes < 1024 * 1024)
                return FString::Printf(TEXT("%.1f KB"), ReceivedBytes / 1024.0f);
            else
                return FString::Printf(TEXT("%.1f MB"), ReceivedBytes / (1024.0f * 1024.0f));
        }

        // 获取总大小字符串
        FString GetTotalSizeString() const
        {
            if (TotalBytes < 1024)
                return FString::Printf(TEXT("%lld B"), TotalBytes);
            else if (TotalBytes < 1024 * 1024)
                return FString::Printf(TEXT("%.1f KB"), TotalBytes / 1024.0f);
            else
                return FString::Printf(TEXT("%.1f MB"), TotalBytes / (1024.0f * 1024.0f));
        }
    };
}

// 下载进度委托
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDownloadProgress, const Download::FDownloadItem&);
// 下载完成委托
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDownloadComplete, bool /* bSuccess */, const FString& /* FilePath */);
// 下载项完成委托
DECLARE_DELEGATE_OneParam(FOnDownloadItemComplete, const Download::FDownloadItem&);