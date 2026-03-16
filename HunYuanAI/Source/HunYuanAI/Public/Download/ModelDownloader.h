#pragma once

#include "CoreMinimal.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/Queue.h"
#include "Containers/Map.h"
#include "Containers/Array.h"
#include "Templates/SharedPointer.h"
#include "HAL/CriticalSection.h"
#include "Async/Async.h"
#include "DownloadTypes.h"

class HUNYUANAI_API FModelDownloader : public TSharedFromThis<FModelDownloader>
{
public:
    FModelDownloader();
    virtual ~FModelDownloader();

    // 添加下载任务
    bool AddDownload(const FString& URL, const FString& JobId, const FString& DownloadDir,
        FOnDownloadItemComplete OnComplete);

    // 取消下载
    void CancelDownload(const FString& JobId);

    // 取消所有下载
    void CancelAllDownloads();

    // 获取下载项状态
    TSharedPtr<Download::FDownloadItem> GetDownloadItem(const FString& JobId) const;

    // 获取所有下载项
    TArray<TSharedPtr<Download::FDownloadItem>> GetAllDownloadItems() const;

    // 是否正在下载
    bool IsDownloading() const { return ActiveDownloads > 0; }

    // 获取活跃下载数
    int32 GetActiveDownloadCount() const { return ActiveDownloads; }

    // 事件
    FOnDownloadProgress OnDownloadProgress;
    FOnDownloadComplete OnDownloadComplete;

private:
    struct FDownloadTask
    {
        FString JobId;
        FString URL;
        FString DestinationPath;
        TSharedPtr<Download::FDownloadItem> Item;
        TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request;
        FOnDownloadItemComplete Callback;

        // 记录开始时间用于计算速度
        double StartTime = 0.0;
        int64 LastBytes = 0;
        double LastTime = 0.0;
        float Speed = 0.0f;
    };

    void OnRequestProgress64(FHttpRequestPtr Request, uint64 BytesSent, uint64 BytesReceived);
    void OnRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);
    void UpdateTaskProgress(const FString& JobId, int64 BytesReceived, int64 ContentLength);
    void CompleteTask(const FString& JobId, bool bSuccess, const FString& FilePath = FString());
    FString GenerateDestinationPath(const FString& URL, const FString& JobId, const FString& DownloadDir);

    mutable FCriticalSection TasksCriticalSection;
    TMap<FString, TSharedPtr<FDownloadTask>> ActiveTasks;
    TMap<FString, TSharedPtr<Download::FDownloadItem>> CompletedItems;

    std::atomic<int32> ActiveDownloads;
};