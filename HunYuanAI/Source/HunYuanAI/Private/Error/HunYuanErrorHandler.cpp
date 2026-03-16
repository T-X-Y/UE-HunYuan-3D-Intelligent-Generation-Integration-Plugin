#include "Error/HunYuanErrorHandler.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Logging/HunYuanLogging.h"

TSharedPtr<FHunYuanErrorHandler> FHunYuanErrorHandler::Instance = nullptr;
FCriticalSection FHunYuanErrorHandler::InstanceCriticalSection;

TSharedPtr<FHunYuanErrorHandler> FHunYuanErrorHandler::Get()
{
    FScopeLock Lock(&InstanceCriticalSection);
    if (!Instance.IsValid())
    {
        Instance = MakeShareable(new FHunYuanErrorHandler());
    }
    return Instance;
}

void FHunYuanErrorHandler::Shutdown()
{
    FScopeLock Lock(&InstanceCriticalSection);
    Instance.Reset();
}

FHunYuanErrorHandler::FHunYuanErrorHandler()
{
}

void FHunYuanErrorHandler::ReportError(const HunYuanError::FErrorInfo& Error)
{
    // 添加到历史
    {
        FScopeLock Lock(&ErrorsCriticalSection);
        ErrorHistory.Add(Error);

        const int32 MaxHistorySize = 100;
        if (ErrorHistory.Num() > MaxHistorySize)
        {
            ErrorHistory.RemoveAt(0, ErrorHistory.Num() - MaxHistorySize);
        }
    }

    // 记录日志
    UE_LOG(LogHunYuanError, Error, TEXT("[Error %d] %s - %s"),
        static_cast<int32>(Error.Code),
        *Error.Message,
        *Error.Details);

    // 触发错误事件
    OnError.Broadcast(Error);

    // 显示通知
    ShowErrorNotification(Error);
}

void FHunYuanErrorHandler::ReportError(HunYuanError::EErrorCode Code, const FString& Details)
{
    HunYuanError::FErrorInfo Error;
    Error.Code = Code;
    Error.Message = HunYuanError::GetErrorDescription(Code);
    Error.Details = Details;

    ReportError(Error);
}

TArray<HunYuanError::FErrorInfo> FHunYuanErrorHandler::GetRecentErrors(int32 MaxCount) const
{
    FScopeLock Lock(&ErrorsCriticalSection);

    TArray<HunYuanError::FErrorInfo> Result;
    int32 StartIndex = FMath::Max(0, ErrorHistory.Num() - MaxCount);

    for (int32 i = StartIndex; i < ErrorHistory.Num(); i++)
    {
        Result.Add(ErrorHistory[i]);
    }

    return Result;
}

void FHunYuanErrorHandler::ClearErrorHistory()
{
    FScopeLock Lock(&ErrorsCriticalSection);
    ErrorHistory.Empty();
}

void FHunYuanErrorHandler::ShowErrorNotification(const HunYuanError::FErrorInfo& Error)
{
    AsyncTask(ENamedThreads::GameThread, [Error]()
        {
            // 创建通知信息
            FNotificationInfo Info(Error.ToDisplayText());

            // 基本设置
            Info.FadeInDuration = 0.1f;
            Info.FadeOutDuration = 0.5f;
            Info.ExpireDuration = 5.0f;
            Info.bUseThrobber = false;
            Info.bUseSuccessFailIcons = true;
            Info.bFireAndForget = true;

            // 根据错误严重性设置图标
            switch (Error.GetNotificationState())
            {
            case SNotificationItem::CS_Fail:
                Info.Image = FCoreStyle::Get().GetBrush("Icons.Error");
                break;
            case SNotificationItem::CS_Pending:
                Info.Image = FCoreStyle::Get().GetBrush("Icons.Warning");
                break;
            default:
                Info.Image = FCoreStyle::Get().GetBrush("Icons.Info");
                break;
            }

            // 添加按钮（可选）
            if (!Error.GetSuggestion().IsEmpty())
            {
                Info.ButtonDetails.Add(FNotificationButtonInfo(
                    NSLOCTEXT("HunYuanErrorHandler", "ShowSuggestion", "查看建议"),
                    FText::GetEmpty(),
                    FSimpleDelegate::CreateLambda([Suggestion = Error.GetSuggestion()]()
                        {
                            // 可以在这里显示建议对话框
                            UE_LOG(LogHunYuanError, Log, TEXT("Suggestion: %s"), *Suggestion);
                        }),
                    SNotificationItem::CS_None
                ));
            }

            // 添加通知
            auto Notification = FSlateNotificationManager::Get().AddNotification(Info);
            if (Notification.IsValid())
            {
                Notification->SetCompletionState(Error.GetNotificationState());
            }
        });
}