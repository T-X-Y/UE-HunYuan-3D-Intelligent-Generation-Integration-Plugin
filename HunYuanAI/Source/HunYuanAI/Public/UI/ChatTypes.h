#pragma once

#include "CoreMinimal.h"
#include "API/HunYuanAPITypes.h"
#include "API/HunYuanModels.h"
#include "Dom/JsonObject.h"

namespace Chat
{
    // 消息类型
    enum class EMessageType
    {
        System,
        User,
        Assistant,
        Error,
        Progress
    };

    // 聊天消息
    struct FChatMessage
    {
        EMessageType Type = EMessageType::System;
        FString Sender;
        FString Content;
        FDateTime Timestamp = FDateTime::Now();
        TSharedPtr<FJsonObject> Metadata;

        // 是否为用户消息
        bool IsUser() const { return Type == EMessageType::User; }

        // 是否为系统消息
        bool IsSystem() const { return Type == EMessageType::System; }

        // 是否为错误消息
        bool IsError() const { return Type == EMessageType::Error; }

        // 获取显示文本
        FText GetDisplayText() const
        {
            if (Sender.IsEmpty())
            {
                return FText::FromString(Content);
            }
            return FText::FromString(Sender + TEXT(": ") + Content);
        }

        // 获取时间字符串
        FString GetTimeString() const
        {
            return Timestamp.ToString(TEXT("%H:%M:%S"));
        }
    };

    // 生成状态
    enum class EGenerationStatus
    {
        Idle,
        Submitting,
        Waiting,
        Downloading,
        Extracting,
        Completed,
        Failed
    };

    // 生成任务信息
    struct FGenerationTask
    {
        FString JobId;
        FString Prompt;
        FString ImagePath;
        EGenerationStatus Status = EGenerationStatus::Idle;
        float Progress = 0.0f;
        FString StatusMessage;
        TSharedPtr<HunYuanAPI::FJobResult> Result;
        FDateTime StartTime;
        FDateTime EndTime;

        // 获取耗时（秒）
        double GetElapsedTime() const
        {
            if (Status == EGenerationStatus::Completed || Status == EGenerationStatus::Failed)
            {
                return (EndTime - StartTime).GetTotalSeconds();
            }
            return (FDateTime::Now() - StartTime).GetTotalSeconds();
        }

        // 获取状态文本
        FString GetStatusText() const
        {
            switch (Status)
            {
            case EGenerationStatus::Submitting: return TEXT("提交中...");
            case EGenerationStatus::Waiting: return TEXT("生成中...");
            case EGenerationStatus::Downloading: return FString::Printf(TEXT("下载中 %.1f%%"), Progress * 100.0f);
            case EGenerationStatus::Extracting: return TEXT("解压中...");
            case EGenerationStatus::Completed: return TEXT("已完成");
            case EGenerationStatus::Failed: return TEXT("失败");
            default: return TEXT("就绪");
            }
        }
    };
}

// 消息委托
DECLARE_MULTICAST_DELEGATE_OneParam(FOnChatMessageAdded, const Chat::FChatMessage&);
// 任务状态委托
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGenerationTaskUpdated, const Chat::FGenerationTask&);