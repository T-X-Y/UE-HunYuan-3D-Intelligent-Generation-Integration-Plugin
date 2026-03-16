#pragma once

#include "CoreMinimal.h"
#include "HunYuanError.h"
#include "Containers/Queue.h"
#include "HAL/CriticalSection.h"

// 错误事件委托
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHunYuanError, const HunYuanError::FErrorInfo&);

class HUNYUANAI_API FHunYuanErrorHandler : public TSharedFromThis<FHunYuanErrorHandler>
{
public:
    static TSharedPtr<FHunYuanErrorHandler> Get();
    static void Shutdown();

    // 报告错误
    void ReportError(const HunYuanError::FErrorInfo& Error);

    // 报告错误（快捷方式）
    void ReportError(HunYuanError::EErrorCode Code, const FString& Details = FString());

    // 获取最近的错误
    TArray<HunYuanError::FErrorInfo> GetRecentErrors(int32 MaxCount = 10) const;

    // 清除错误历史
    void ClearErrorHistory();

    // 错误事件
    FOnHunYuanError OnError;

    // 显示错误通知（在游戏线程）
    void ShowErrorNotification(const HunYuanError::FErrorInfo& Error);

private:
    FHunYuanErrorHandler();

    static TSharedPtr<FHunYuanErrorHandler> Instance;
    static FCriticalSection InstanceCriticalSection;

    mutable FCriticalSection ErrorsCriticalSection;
    TArray<HunYuanError::FErrorInfo> ErrorHistory;
};