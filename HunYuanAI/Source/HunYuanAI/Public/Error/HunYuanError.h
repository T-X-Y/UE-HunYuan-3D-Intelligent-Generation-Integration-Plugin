#pragma once

#include "CoreMinimal.h"
#include "Widgets/Notifications/SNotificationList.h"  // 添加这个头文件

namespace HunYuanError
{
    // 错误码
    enum class EErrorCode
    {
        Success = 0,

        // 配置错误 (1000-1999)
        InvalidCredentials = 1001,
        ConfigNotFound = 1002,
        ConfigSaveFailed = 1003,

        // 网络错误 (2000-2999)
        NetworkUnavailable = 2001,
        RequestTimeout = 2002,
        InvalidResponse = 2003,

        // API错误 (3000-3999)
        ApiInvalidRequest = 3001,
        ApiAuthenticationFailed = 3002,
        ApiRateLimitExceeded = 3003,
        ApiServerError = 3004,
        ApiJobFailed = 3005,
        ApiJobNotFound = 3006,

        // 文件错误 (4000-4999)
        FileNotFound = 4001,
        FileReadFailed = 4002,
        FileWriteFailed = 4003,
        FileExtractFailed = 4004,
        UnsupportedFormat = 4005,

        // 导入错误 (5000-5999)
        ImportFailed = 5001,
        AssetCreationFailed = 5002,

        // 未知错误 (9999)
        Unknown = 9999
    };

    // 错误严重级别
    enum class EErrorSeverity
    {
        None,
        Info,
        Warning,
        Error,
        Fatal
    };

    // 错误信息
    struct FErrorInfo
    {
        EErrorCode Code = EErrorCode::Success;
        FString Message;
        FString Details;
        FDateTime Timestamp = FDateTime::Now();

        bool IsSuccess() const { return Code == EErrorCode::Success; }
        bool IsError() const { return Code != EErrorCode::Success; }

        // 这些函数在cpp文件中实现
        FText ToDisplayText() const;
        FString ToString() const;
        FText ToDetailedText() const;
        ELogVerbosity::Type GetLogVerbosity() const;
        SNotificationItem::ECompletionState GetNotificationState() const;
        FString GetSuggestion() const;

        static FErrorInfo Success()
        {
            return FErrorInfo{ EErrorCode::Success, TEXT(""), TEXT("") };
        }

        static FErrorInfo FromException(const std::exception& e)
        {
            return FErrorInfo{
                EErrorCode::Unknown,
                UTF8_TO_TCHAR(e.what()),
                TEXT("C++ Exception")
            };
        }

        static FErrorInfo FromException(const FString& ExceptionMessage)
        {
            return FErrorInfo{
                EErrorCode::Unknown,
                ExceptionMessage,
                TEXT("UE Exception")
            };
        }

        static FErrorInfo FromHttpStatus(int32 HttpCode, const FString& Message);
    };

    // 全局函数声明
    FString GetErrorDescription(EErrorCode Code);
    EErrorSeverity GetErrorSeverity(EErrorCode Code);
    bool IsRetryable(EErrorCode Code);
    FErrorInfo FromHttpStatus(int32 HttpCode, const FString& Message);
}

// 序列化支持
FString LexToString(HunYuanError::EErrorCode Code);
bool LexTryParseString(HunYuanError::EErrorCode& OutCode, const TCHAR* Buffer);
void LexFromString(HunYuanError::EErrorCode& OutValue, const TCHAR* Buffer);