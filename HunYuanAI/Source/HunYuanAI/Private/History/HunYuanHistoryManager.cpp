#include "History/HunYuanHistoryManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogHunYuanHistory, Log, All);

TSharedPtr<FHunYuanHistoryManager> FHunYuanHistoryManager::Instance = nullptr;
FCriticalSection FHunYuanHistoryManager::InstanceCriticalSection;

// FHistoryEntry 的序列化方法实现
TSharedPtr<FJsonObject> FHistoryEntry::ToJson() const
{
    TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject());

    Json->SetStringField(TEXT("EntryId"), EntryId);
    Json->SetNumberField(TEXT("Type"), static_cast<int32>(Type));
    Json->SetStringField(TEXT("Timestamp"), Timestamp.ToString());
    Json->SetStringField(TEXT("Description"), Description);
    Json->SetNumberField(TEXT("Status"), static_cast<int32>(Status));
    Json->SetNumberField(TEXT("Progress"), Progress);
    Json->SetStringField(TEXT("JobId"), JobId);
    Json->SetStringField(TEXT("UserName"), UserName);
    Json->SetStringField(TEXT("SessionId"), SessionId);
    Json->SetNumberField(TEXT("Duration"), Duration);
    Json->SetNumberField(TEXT("MemoryUsage"), MemoryUsage);

    return Json;
}

bool FHistoryEntry::FromJson(const TSharedPtr<FJsonObject>& Json)
{
    if (!Json.IsValid()) return false;

    Json->TryGetStringField(TEXT("EntryId"), EntryId);

    int32 TypeInt;
    if (Json->TryGetNumberField(TEXT("Type"), TypeInt))
    {
        Type = static_cast<EHistoryEntryType>(TypeInt);
    }

    FString TimeStr;
    if (Json->TryGetStringField(TEXT("Timestamp"), TimeStr))
    {
        FDateTime::Parse(TimeStr, Timestamp);
    }

    Json->TryGetStringField(TEXT("Description"), Description);

    int32 StatusInt;
    if (Json->TryGetNumberField(TEXT("Status"), StatusInt))
    {
        Status = static_cast<EHistoryTaskStatus>(StatusInt);
    }

    Json->TryGetNumberField(TEXT("Progress"), Progress);
    Json->TryGetStringField(TEXT("JobId"), JobId);
    Json->TryGetStringField(TEXT("UserName"), UserName);
    Json->TryGetStringField(TEXT("SessionId"), SessionId);
    Json->TryGetNumberField(TEXT("Duration"), Duration);
    Json->TryGetNumberField(TEXT("MemoryUsage"), MemoryUsage);

    return true;
}

TSharedPtr<FJsonObject> FDownloadHistoryEntry::ToJson() const
{
    auto Json = FHistoryEntry::ToJson();

    Json->SetStringField(TEXT("URL"), URL);
    Json->SetStringField(TEXT("LocalPath"), LocalPath);
    Json->SetNumberField(TEXT("FileSize"), FileSize);
    Json->SetNumberField(TEXT("DownloadedSize"), DownloadedSize);
    Json->SetNumberField(TEXT("DownloadSpeed"), DownloadSpeed);
    Json->SetNumberField(TEXT("RetryCount"), RetryCount);
    Json->SetStringField(TEXT("FileHash"), FileHash);

    return Json;
}

bool FDownloadHistoryEntry::FromJson(const TSharedPtr<FJsonObject>& Json)
{
    if (!FHistoryEntry::FromJson(Json)) return false;

    Json->TryGetStringField(TEXT("URL"), URL);
    Json->TryGetStringField(TEXT("LocalPath"), LocalPath);
    Json->TryGetNumberField(TEXT("FileSize"), FileSize);
    Json->TryGetNumberField(TEXT("DownloadedSize"), DownloadedSize);
    Json->TryGetNumberField(TEXT("DownloadSpeed"), DownloadSpeed);
    Json->TryGetNumberField(TEXT("RetryCount"), RetryCount);
    Json->TryGetStringField(TEXT("FileHash"), FileHash);

    return true;
}

TSharedPtr<FJsonObject> FAPIHistoryEntry::ToJson() const
{
    auto Json = FHistoryEntry::ToJson();

    Json->SetStringField(TEXT("Action"), Action);
    Json->SetNumberField(TEXT("HttpCode"), HttpCode);
    Json->SetStringField(TEXT("Endpoint"), Endpoint);
    Json->SetNumberField(TEXT("RequestTime"), RequestTime);

    return Json;
}

bool FAPIHistoryEntry::FromJson(const TSharedPtr<FJsonObject>& Json)
{
    if (!FHistoryEntry::FromJson(Json)) return false;

    Json->TryGetStringField(TEXT("Action"), Action);
    Json->TryGetNumberField(TEXT("HttpCode"), HttpCode);
    Json->TryGetStringField(TEXT("Endpoint"), Endpoint);
    Json->TryGetNumberField(TEXT("RequestTime"), RequestTime);

    return true;
}

// FHunYuanHistoryManager 实现
TSharedPtr<FHunYuanHistoryManager> FHunYuanHistoryManager::Get()
{
    FScopeLock Lock(&InstanceCriticalSection);
    if (!Instance.IsValid())
    {
        Instance = MakeShareable(new FHunYuanHistoryManager());
        Instance->LoadHistory();
    }
    return Instance;
}

void FHunYuanHistoryManager::Shutdown()
{
    FScopeLock Lock(&InstanceCriticalSection);
    if (Instance.IsValid())
    {
        Instance->SaveHistory();
        Instance.Reset();
    }
}

FHunYuanHistoryManager::FHunYuanHistoryManager()
{
    HistoryFilePath = GetHistoryFilePath();

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    FString HistoryDir = FPaths::GetPath(HistoryFilePath);
    if (!PlatformFile.DirectoryExists(*HistoryDir))
    {
        PlatformFile.CreateDirectoryTree(*HistoryDir);
    }
}

FHunYuanHistoryManager::~FHunYuanHistoryManager()
{
    SaveHistory();
}

void FHunYuanHistoryManager::AddEntry(TSharedPtr<FHistoryEntry> Entry)
{
    FScopeLock Lock(&HistoryCriticalSection);
    AddEntryInternal(Entry);
}

void FHunYuanHistoryManager::AddDownloadEntry(const FDownloadHistoryEntry& Entry)
{
    FScopeLock Lock(&HistoryCriticalSection);
    auto EntryPtr = MakeShareable(new FDownloadHistoryEntry(Entry));
    AddEntryInternal(EntryPtr);
}

void FHunYuanHistoryManager::AddAPIEntry(const FAPIHistoryEntry& Entry)
{
    FScopeLock Lock(&HistoryCriticalSection);
    auto EntryPtr = MakeShareable(new FAPIHistoryEntry(Entry));
    AddEntryInternal(EntryPtr);
}

void FHunYuanHistoryManager::AddEntryInternal(TSharedPtr<FHistoryEntry> Entry)
{
    Entries.Insert(Entry, 0);

    if (!Entry->JobId.IsEmpty())
    {
        JobIdIndex.Add(Entry->JobId, Entry);
    }

    CleanupOldEntries();

    OnHistoryEntryAdded.Broadcast(*Entry);
    OnHistoryChanged.Broadcast();

    if (bAutoSave)
    {
        SaveHistory();
    }
}

TArray<TSharedPtr<FHistoryEntry>> FHunYuanHistoryManager::GetAllEntries() const
{
    FScopeLock Lock(&HistoryCriticalSection);
    return Entries;
}

TArray<TSharedPtr<FHistoryEntry>> FHunYuanHistoryManager::GetEntriesByType(EHistoryEntryType Type) const
{
    FScopeLock Lock(&HistoryCriticalSection);

    TArray<TSharedPtr<FHistoryEntry>> Result;
    for (const auto& Entry : Entries)
    {
        if (Entry->Type == Type)
        {
            Result.Add(Entry);
        }
    }
    return Result;
}

TArray<TSharedPtr<FHistoryEntry>> FHunYuanHistoryManager::GetRecentEntries(int32 Count) const
{
    FScopeLock Lock(&HistoryCriticalSection);

    TArray<TSharedPtr<FHistoryEntry>> Result;
    int32 NumToTake = FMath::Min(Count, Entries.Num());
    for (int32 i = 0; i < NumToTake; i++)
    {
        Result.Add(Entries[i]);
    }
    return Result;
}

TSharedPtr<FHistoryEntry> FHunYuanHistoryManager::GetEntryByJobId(const FString& JobId) const
{
    FScopeLock Lock(&HistoryCriticalSection);

    const TSharedPtr<FHistoryEntry>* Found = JobIdIndex.Find(JobId);
    if (Found)
    {
        return *Found;
    }
    return nullptr;
}

FHistoryStatistics FHunYuanHistoryManager::GetStatistics() const
{
    FScopeLock Lock(&HistoryCriticalSection);

    FHistoryStatistics Stats;

    for (const auto& Entry : Entries)
    {
        Stats.TotalEntries++;
        Stats.TotalDuration += Entry->Duration;

        switch (Entry->Type)
        {
        case EHistoryEntryType::Generation: Stats.GenerationCount++; break;
        case EHistoryEntryType::Import: Stats.ImportCount++; break;
        case EHistoryEntryType::Download: Stats.DownloadCount++; break;
        case EHistoryEntryType::API: Stats.APICount++; break;
        case EHistoryEntryType::Config: Stats.ConfigCount++; break;
        case EHistoryEntryType::Error: Stats.ErrorCount++; break;
        }

        if (Entry->Status == EHistoryTaskStatus::Completed)
        {
            Stats.SuccessCount++;
        }
        else if (Entry->Status == EHistoryTaskStatus::Failed)
        {
            Stats.FailedCount++;
        }
    }

    if (Stats.TotalEntries > 0)
    {
        Stats.AverageDuration = Stats.TotalDuration / Stats.TotalEntries;
    }

    return Stats;
}

void FHunYuanHistoryManager::ClearHistory()
{
    FScopeLock Lock(&HistoryCriticalSection);

    Entries.Empty();
    JobIdIndex.Empty();

    OnHistoryCleared.Broadcast();
    OnHistoryChanged.Broadcast();

    if (bAutoSave)
    {
        SaveHistory();
    }
}

void FHunYuanHistoryManager::SetMaxHistoryCount(int32 MaxCount)
{
    FScopeLock Lock(&HistoryCriticalSection);
    MaxHistoryCount = MaxCount;
    CleanupOldEntries();
}

void FHunYuanHistoryManager::CleanupOldEntries()
{
    if (Entries.Num() > MaxHistoryCount)
    {
        int32 RemoveCount = Entries.Num() - MaxHistoryCount;
        Entries.RemoveAt(Entries.Num() - RemoveCount, RemoveCount);
    }
}

FString FHunYuanHistoryManager::GetHistoryFilePath()
{
    return FPaths::ProjectSavedDir() / TEXT("HunYuanAI") / TEXT("History.json");
}

void FHunYuanHistoryManager::LoadHistory()
{
    FScopeLock Lock(&HistoryCriticalSection);

    if (!FPaths::FileExists(HistoryFilePath))
    {
        return;
    }

    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *HistoryFilePath))
    {
        return;
    }

    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* EntriesArray;
    if (RootObject->TryGetArrayField(TEXT("Entries"), EntriesArray))
    {
        for (const auto& Item : *EntriesArray)
        {
            TSharedPtr<FJsonObject> EntryObj = Item->AsObject();
            if (!EntryObj.IsValid()) continue;

            int32 TypeInt;
            if (!EntryObj->TryGetNumberField(TEXT("Type"), TypeInt)) continue;

            TSharedPtr<FHistoryEntry> Entry;
            EHistoryEntryType Type = static_cast<EHistoryEntryType>(TypeInt);

            if (Type == EHistoryEntryType::Download)
            {
                auto DownloadEntry = MakeShared<FDownloadHistoryEntry>();
                if (DownloadEntry->FromJson(EntryObj))  // 使用 -> 是正确的
                {
                    Entry = DownloadEntry;
                }
            }
            else if (Type == EHistoryEntryType::API)
            {
                auto APIEntry = MakeShared<FAPIHistoryEntry>();
                if (APIEntry->FromJson(EntryObj))  // 使用 -> 是正确的
                {
                    Entry = APIEntry;
                }
            }

            if (Entry.IsValid())
            {
                AddEntryInternal(Entry);
            }
        }
    }
}

void FHunYuanHistoryManager::SaveHistory() const
{
    FScopeLock Lock(&HistoryCriticalSection);

    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject());
    TArray<TSharedPtr<FJsonValue>> EntriesArray;

    for (const auto& Entry : Entries)
    {
        EntriesArray.Add(MakeShareable(new FJsonValueObject(Entry->ToJson())));
    }

    RootObject->SetArrayField(TEXT("Entries"), EntriesArray);

    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    if (FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
    {
        FFileHelper::SaveStringToFile(JsonString, *HistoryFilePath);
    }
}