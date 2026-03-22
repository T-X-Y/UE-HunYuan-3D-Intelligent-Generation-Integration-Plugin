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

        float GetProgressPercent() const
        {
            return TotalBytes > 0 ? (float)ReceivedBytes / TotalBytes : 0.0f;
        }

        FString GetReceivedSizeString() const
        {
            if (ReceivedBytes < 1024)
                return FString::Printf(TEXT("%lld B"), ReceivedBytes);
            else if (ReceivedBytes < 1024 * 1024)
                return FString::Printf(TEXT("%.1f KB"), ReceivedBytes / 1024.0f);
            else
                return FString::Printf(TEXT("%.1f MB"), ReceivedBytes / (1024.0f * 1024.0f));
        }

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

// 委托定义
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDownloadProgress, const Download::FDownloadItem&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDownloadComplete, bool /* bSuccess */, const FString& /* FilePath */);
DECLARE_DELEGATE_OneParam(FOnDownloadItemComplete, const Download::FDownloadItem&);