#include "Download/ModelDownloader.h"
#include "Download/ModelURLParser.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Logging/HunYuanLogging.h"

FModelDownloader::FModelDownloader()
    : ActiveDownloads(0)
{
}

FModelDownloader::~FModelDownloader()
{
    CancelAllDownloads();
}

bool FModelDownloader::AddDownload(const FString& URL, const FString& JobId, const FString& DownloadDir,
    FOnDownloadItemComplete OnComplete)
{
    // 检查URL有效性
    if (URL.IsEmpty() || !URL.StartsWith(TEXT("http")))
    {
        UE_LOG(LogHunYuanDownload, Error, TEXT("Invalid URL: %s"), *URL);
        return false;
    }

    FString DestinationPath = GenerateDestinationPath(URL, JobId, DownloadDir);

    // 创建下载项
    auto Item = MakeShared<Download::FDownloadItem>();
    Item->URL = URL;
    Item->JobId = JobId;
    Item->DestinationPath = DestinationPath;
    Item->FileName = FPaths::GetCleanFilename(DestinationPath);
    Item->Status = Download::EDownloadStatus::Downloading;

    // 创建HTTP请求
    auto Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(URL);
    Request->SetVerb(TEXT("GET"));
    Request->SetTimeout(300); // 5分钟超时

    // 创建任务
    auto Task = MakeShared<FDownloadTask>();
    Task->JobId = JobId;
    Task->URL = URL;
    Task->DestinationPath = DestinationPath;
    Task->Item = Item;
    Task->Request = Request;
    Task->Callback = OnComplete;
    Task->StartTime = FPlatformTime::Seconds();
    Task->LastTime = Task->StartTime;

    // 绑定回调
    Request->OnRequestProgress64().BindRaw(this, &FModelDownloader::OnRequestProgress64);
    Request->OnProcessRequestComplete().BindRaw(this, &FModelDownloader::OnRequestComplete);

    {
        FScopeLock Lock(&TasksCriticalSection);
        ActiveTasks.Add(JobId, Task);
    }

    ActiveDownloads++;

    UE_LOG(LogHunYuanDownload, Log, TEXT("Download started: %s -> %s"), *URL, *DestinationPath);

    // 启动请求
    return Request->ProcessRequest();
}

void FModelDownloader::CancelDownload(const FString& JobId)
{
    FScopeLock Lock(&TasksCriticalSection);

    if (auto* TaskPtr = ActiveTasks.Find(JobId))
    {
        auto Task = *TaskPtr;
        if (Task->Request.IsValid() && Task->Request->GetStatus() == EHttpRequestStatus::Processing)
        {
            Task->Request->CancelRequest();
        }

        Task->Item->Status = Download::EDownloadStatus::Cancelled;
        CompletedItems.Add(JobId, Task->Item);
        ActiveTasks.Remove(JobId);
        ActiveDownloads--;

        UE_LOG(LogHunYuanDownload, Log, TEXT("Download cancelled: %s"), *JobId);

        // 回调
        AsyncTask(ENamedThreads::GameThread, [Callback = Task->Callback, Item = Task->Item]()
            {
                Callback.ExecuteIfBound(*Item);
            });
    }
}

void FModelDownloader::CancelAllDownloads()
{
    FScopeLock Lock(&TasksCriticalSection);

    TArray<FString> JobIds;
    ActiveTasks.GetKeys(JobIds);

    for (const FString& JobId : JobIds)
    {
        if (auto* TaskPtr = ActiveTasks.Find(JobId))
        {
            auto Task = *TaskPtr;
            if (Task->Request.IsValid())
            {
                Task->Request->CancelRequest();
            }
            Task->Item->Status = Download::EDownloadStatus::Cancelled;
        }
    }

    ActiveTasks.Empty();
    ActiveDownloads = 0;

    UE_LOG(LogHunYuanDownload, Log, TEXT("All downloads cancelled"));
}

TSharedPtr<Download::FDownloadItem> FModelDownloader::GetDownloadItem(const FString& JobId) const
{
    FScopeLock Lock(&TasksCriticalSection);

    if (auto* TaskPtr = ActiveTasks.Find(JobId))
    {
        return (*TaskPtr)->Item;
    }

    if (auto* ItemPtr = CompletedItems.Find(JobId))
    {
        return *ItemPtr;
    }

    return nullptr;
}

TArray<TSharedPtr<Download::FDownloadItem>> FModelDownloader::GetAllDownloadItems() const
{
    TArray<TSharedPtr<Download::FDownloadItem>> Result;

    FScopeLock Lock(&TasksCriticalSection);

    for (const auto& Pair : ActiveTasks)
    {
        Result.Add(Pair.Value->Item);
    }

    for (const auto& Pair : CompletedItems)
    {
        Result.Add(Pair.Value);
    }

    return Result;
}

void FModelDownloader::OnRequestProgress64(FHttpRequestPtr Request, uint64 BytesSent, uint64 BytesReceived)
{
    // 找到对应的任务
    FString JobId;
    {
        FScopeLock Lock(&TasksCriticalSection);
        for (const auto& Pair : ActiveTasks)
        {
            if (Pair.Value->Request == Request)
            {
                JobId = Pair.Key;
                break;
            }
        }
    }

    if (!JobId.IsEmpty())
    {
        int32 ContentLength = Request->GetResponse().IsValid() ?
            Request->GetResponse()->GetContentLength() : 0;
        UpdateTaskProgress(JobId, static_cast<int64>(BytesReceived), static_cast<int64>(ContentLength));
    }
}

void FModelDownloader::UpdateTaskProgress(const FString& JobId, int64 BytesReceived, int64 ContentLength)
{
    FScopeLock Lock(&TasksCriticalSection);

    if (auto* TaskPtr = ActiveTasks.Find(JobId))
    {
        auto Task = *TaskPtr;
        auto& Item = Task->Item;

        double CurrentTime = FPlatformTime::Seconds();

        Item->ReceivedBytes = BytesReceived;
        if (ContentLength > 0)
        {
            Item->TotalBytes = ContentLength;
            Item->Progress = (float)BytesReceived / ContentLength;
        }

        // 计算下载速度
        if (CurrentTime - Task->LastTime >= 1.0) // 每秒更新一次
        {
            int64 BytesDelta = BytesReceived - Task->LastBytes;
            double TimeDelta = CurrentTime - Task->LastTime;
            if (TimeDelta > 0)
            {
                Task->Speed = static_cast<float>(BytesDelta / TimeDelta);
            }

            Task->LastBytes = BytesReceived;
            Task->LastTime = CurrentTime;
        }

        Item->Status = Download::EDownloadStatus::Downloading;

        // 触发进度事件（在游戏线程）
        AsyncTask(ENamedThreads::GameThread, [WeakThis = TWeakPtr<FModelDownloader>(AsShared()), Item]()
            {
                auto SharedThis = WeakThis.Pin();
                if (SharedThis.IsValid())
                {
                    SharedThis->OnDownloadProgress.Broadcast(*Item);
                }
            });
    }
}

void FModelDownloader::OnRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
    FString JobId;
    TSharedPtr<FDownloadTask> FoundTask;

    {
        FScopeLock Lock(&TasksCriticalSection);
        for (const auto& Pair : ActiveTasks)
        {
            if (Pair.Value->Request == Request)
            {
                JobId = Pair.Key;
                FoundTask = Pair.Value;
                break;
            }
        }
    }

    if (!FoundTask.IsValid())
    {
        return;
    }

    bool bSuccess = false;
    FString FilePath;

    if (bConnectedSuccessfully && Response.IsValid())
    {
        int32 ResponseCode = Response->GetResponseCode();
        if (ResponseCode == 200)
        {
            const TArray<uint8>& ResponseData = Response->GetContent();
            if (FFileHelper::SaveArrayToFile(ResponseData, *FoundTask->DestinationPath))
            {
                bSuccess = true;
                FilePath = FoundTask->DestinationPath;

                // 更新最终大小
                FoundTask->Item->TotalBytes = ResponseData.Num();
                FoundTask->Item->ReceivedBytes = ResponseData.Num();
                FoundTask->Item->Progress = 1.0f;

                UE_LOG(LogHunYuanDownload, Log, TEXT("Download completed: %s (%lld bytes)"),
                    *FilePath, ResponseData.Num());
            }
            else
            {
                UE_LOG(LogHunYuanDownload, Error, TEXT("Failed to save file: %s"), *FoundTask->DestinationPath);
            }
        }
        else
        {
            UE_LOG(LogHunYuanDownload, Error, TEXT("HTTP Error %d for URL: %s"), ResponseCode, *FoundTask->URL);
        }
    }
    else
    {
        UE_LOG(LogHunYuanDownload, Error, TEXT("Connection failed for URL: %s"), *FoundTask->URL);
    }

    CompleteTask(JobId, bSuccess, FilePath);
}

void FModelDownloader::CompleteTask(const FString& JobId, bool bSuccess, const FString& FilePath)
{
    TSharedPtr<FDownloadTask> CompletedTask;

    {
        FScopeLock Lock(&TasksCriticalSection);
        if (auto* TaskPtr = ActiveTasks.Find(JobId))
        {
            CompletedTask = *TaskPtr;

            CompletedTask->Item->Status = bSuccess ? Download::EDownloadStatus::Completed :
                Download::EDownloadStatus::Failed;
            if (!bSuccess)
            {
                CompletedTask->Item->ErrorMessage = TEXT("Download failed");
            }

            CompletedItems.Add(JobId, CompletedTask->Item);
            ActiveTasks.Remove(JobId);
            ActiveDownloads--;
        }
    }

    if (CompletedTask.IsValid())
    {
        // 触发完成事件（在游戏线程）
        AsyncTask(ENamedThreads::GameThread,
            [WeakThis = TWeakPtr<FModelDownloader>(AsShared()), bSuccess, FilePath, Task = CompletedTask]()
            {
                auto SharedThis = WeakThis.Pin();
                if (SharedThis.IsValid())
                {
                    SharedThis->OnDownloadComplete.Broadcast(bSuccess, FilePath);
                    Task->Callback.ExecuteIfBound(*Task->Item);
                }
            });
    }
}

FString FModelDownloader::GenerateDestinationPath(const FString& URL, const FString& JobId, const FString& DownloadDir)
{
    FString FileName;

    // 尝试从URL获取文件名
    int32 LastSlashIndex = -1;
    if (URL.FindLastChar('/', LastSlashIndex))
    {
        FString URLFileName = URL.RightChop(LastSlashIndex + 1);
        // 移除URL参数
        int32 QueryPos = URLFileName.Find(TEXT("?"));
        if (QueryPos != INDEX_NONE)
        {
            URLFileName = URLFileName.Left(QueryPos);
        }

        if (!URLFileName.IsEmpty() && URLFileName.Contains(TEXT(".")))
        {
            FileName = URLFileName;
        }
    }

    // 如果没有有效的文件名，生成一个
    if (FileName.IsEmpty())
    {
        FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
        FileName = FString::Printf(TEXT("%s_%s.model"), *JobId, *Timestamp);
    }

    // 确保目录存在
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*DownloadDir))
    {
        PlatformFile.CreateDirectoryTree(*DownloadDir);
    }

    return DownloadDir / FileName;
}