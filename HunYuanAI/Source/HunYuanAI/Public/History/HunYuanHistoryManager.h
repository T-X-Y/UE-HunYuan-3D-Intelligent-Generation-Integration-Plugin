#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Dom/JsonObject.h"
#include "HunYuanHistoryTypes.h"

// 委托定义 - 使用简单类型
DECLARE_MULTICAST_DELEGATE(FOnHistoryChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHistoryEntryAdded, const FHistoryEntry&);
DECLARE_MULTICAST_DELEGATE(FOnHistoryCleared);

class HUNYUANAI_API FHunYuanHistoryManager : public TSharedFromThis<FHunYuanHistoryManager>
{
public:
    static TSharedPtr<FHunYuanHistoryManager> Get();
    static void Shutdown();

    ~FHunYuanHistoryManager();

    // 基础操作
    void AddEntry(TSharedPtr<FHistoryEntry> Entry);
    void AddDownloadEntry(const FDownloadHistoryEntry& Entry);
    void AddAPIEntry(const FAPIHistoryEntry& Entry);

    // 查询功能
    TArray<TSharedPtr<FHistoryEntry>> GetAllEntries() const;
    TArray<TSharedPtr<FHistoryEntry>> GetEntriesByType(EHistoryEntryType Type) const;
    TArray<TSharedPtr<FHistoryEntry>> GetRecentEntries(int32 Count = 50) const;
    TSharedPtr<FHistoryEntry> GetEntryByJobId(const FString& JobId) const;

    // 统计功能
    FHistoryStatistics GetStatistics() const;

    // 管理功能
    void ClearHistory();
    void SetMaxHistoryCount(int32 MaxCount);
    int32 GetEntryCount() const { return Entries.Num(); }

    // 持久化
    void LoadHistory();
    void SaveHistory() const;
    static FString GetHistoryFilePath();

    // 委托
    FOnHistoryChanged OnHistoryChanged;
    FOnHistoryEntryAdded OnHistoryEntryAdded;
    FOnHistoryCleared OnHistoryCleared;

private:
    FHunYuanHistoryManager();

    void AddEntryInternal(TSharedPtr<FHistoryEntry> Entry);
    void CleanupOldEntries();

private:
    static TSharedPtr<FHunYuanHistoryManager> Instance;
    static FCriticalSection InstanceCriticalSection;

    mutable FCriticalSection HistoryCriticalSection;

    TArray<TSharedPtr<FHistoryEntry>> Entries;
    TMap<FString, TSharedPtr<FHistoryEntry>> JobIdIndex;

    int32 MaxHistoryCount = 1000;
    bool bAutoSave = true;
    FString HistoryFilePath;
};