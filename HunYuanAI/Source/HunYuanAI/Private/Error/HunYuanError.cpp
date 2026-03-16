#include "Error/HunYuanError.h"
#include "Widgets/Notifications/SNotificationList.h"  // 添加这个头文件

namespace HunYuanError
{
    // 获取错误码的详细描述
    FString GetErrorDescription(EErrorCode Code)
    {
        switch (Code)
        {
        case EErrorCode::Success:
            return TEXT("成功");

            // 配置错误 (1000-1999)
        case EErrorCode::InvalidCredentials:
            return TEXT("无效的API凭证，请检查SecretId和SecretKey是否正确");
        case EErrorCode::ConfigNotFound:
            return TEXT("配置文件不存在，将使用默认配置");
        case EErrorCode::ConfigSaveFailed:
            return TEXT("保存配置文件失败，请检查磁盘空间和写入权限");

            // 网络错误 (2000-2999)
        case EErrorCode::NetworkUnavailable:
            return TEXT("网络不可用，请检查网络连接");
        case EErrorCode::RequestTimeout:
            return TEXT("请求超时，服务器响应时间过长");
        case EErrorCode::InvalidResponse:
            return TEXT("服务器返回了无效的响应数据");

            // API错误 (3000-3999)
        case EErrorCode::ApiInvalidRequest:
            return TEXT("API请求参数无效");
        case EErrorCode::ApiAuthenticationFailed:
            return TEXT("API认证失败，请检查SecretId和SecretKey是否有效");
        case EErrorCode::ApiRateLimitExceeded:
            return TEXT("API调用频率超限，请稍后重试");
        case EErrorCode::ApiServerError:
            return TEXT("API服务器内部错误");
        case EErrorCode::ApiJobFailed:
            return TEXT("任务执行失败");
        case EErrorCode::ApiJobNotFound:
            return TEXT("任务ID不存在");

            // 文件错误 (4000-4999)
        case EErrorCode::FileNotFound:
            return TEXT("文件不存在");
        case EErrorCode::FileReadFailed:
            return TEXT("读取文件失败");
        case EErrorCode::FileWriteFailed:
            return TEXT("写入文件失败，请检查磁盘空间");
        case EErrorCode::FileExtractFailed:
            return TEXT("解压文件失败，文件可能已损坏");
        case EErrorCode::UnsupportedFormat:
            return TEXT("不支持的3D模型格式");

            // 导入错误 (5000-5999)
        case EErrorCode::ImportFailed:
            return TEXT("导入模型到UE失败");
        case EErrorCode::AssetCreationFailed:
            return TEXT("创建资源失败");

            // 未知错误 (9999)
        case EErrorCode::Unknown:
            return TEXT("发生未知错误");

        default:
            return TEXT("未定义的错误码");
        }
    }

    // 获取错误码的严重级别
    EErrorSeverity GetErrorSeverity(EErrorCode Code)
    {
        switch (Code)
        {
        case EErrorCode::Success:
            return EErrorSeverity::None;

            // 信息性错误
        case EErrorCode::ConfigNotFound:
            return EErrorSeverity::Info;

            // 警告性错误
        case EErrorCode::ApiRateLimitExceeded:
        case EErrorCode::RequestTimeout:
            return EErrorSeverity::Warning;

            // 严重错误
        case EErrorCode::InvalidCredentials:
        case EErrorCode::ConfigSaveFailed:
        case EErrorCode::NetworkUnavailable:
        case EErrorCode::ApiAuthenticationFailed:
        case EErrorCode::ApiServerError:
        case EErrorCode::FileNotFound:
        case EErrorCode::FileReadFailed:
        case EErrorCode::FileWriteFailed:
        case EErrorCode::FileExtractFailed:
        case EErrorCode::ImportFailed:
        case EErrorCode::AssetCreationFailed:
            return EErrorSeverity::Error;

            // 致命错误
        case EErrorCode::ApiInvalidRequest:
        case EErrorCode::Unknown:
            return EErrorSeverity::Fatal;

        default:
            return EErrorSeverity::Error;
        }
    }

    // 检查错误是否可以重试
    bool IsRetryable(EErrorCode Code)
    {
        switch (Code)
        {
        case EErrorCode::RequestTimeout:
        case EErrorCode::ApiRateLimitExceeded:
        case EErrorCode::ApiServerError:
        case EErrorCode::NetworkUnavailable:
            return true;

        default:
            return false;
        }
    }

    // 从HTTP状态码创建错误
    FErrorInfo FromHttpStatus(int32 HttpCode, const FString& Message)
    {
        FErrorInfo Error;
        Error.Code = EErrorCode::Unknown;
        Error.Message = Message;
        Error.Details = FString::Printf(TEXT("HTTP %d: %s"), HttpCode, *Message);

        switch (HttpCode)
        {
        case 400:
            Error.Code = EErrorCode::ApiInvalidRequest;
            break;
        case 401:
        case 403:
            Error.Code = EErrorCode::ApiAuthenticationFailed;
            break;
        case 429:
            Error.Code = EErrorCode::ApiRateLimitExceeded;
            break;
        case 500:
        case 502:
        case 503:
        case 504:
            Error.Code = EErrorCode::ApiServerError;
            break;
        }

        return Error;
    }

    // FErrorInfo 成员函数实现
    FString FErrorInfo::ToString() const
    {
        if (Details.IsEmpty())
        {
            return FString::Printf(TEXT("[%d] %s"), static_cast<int32>(Code), *Message);
        }
        else
        {
            return FString::Printf(TEXT("[%d] %s - %s"), static_cast<int32>(Code), *Message, *Details);
        }
    }

    FText FErrorInfo::ToDisplayText() const
    {
        if (Message.IsEmpty())
        {
            return FText::Format(
                NSLOCTEXT("HunYuanError", "ErrorCodeFormat", "错误 [{0}]"),
                FText::AsNumber(static_cast<int32>(Code))
            );
        }
        return FText::FromString(Message);
    }

    FText FErrorInfo::ToDetailedText() const
    {
        FString Combined = FString::Printf(TEXT("%s (错误码: %d)"),
            *Message,
            static_cast<int32>(Code));

        if (!Details.IsEmpty())
        {
            Combined += TEXT("\n") + Details;
        }

        return FText::FromString(Combined);
    }

    ELogVerbosity::Type FErrorInfo::GetLogVerbosity() const
    {
        switch (GetErrorSeverity(Code))
        {
        case EErrorSeverity::Info:
            return ELogVerbosity::Log;
        case EErrorSeverity::Warning:
            return ELogVerbosity::Warning;
        case EErrorSeverity::Error:
            return ELogVerbosity::Error;
        case EErrorSeverity::Fatal:
            return ELogVerbosity::Fatal;
        default:
            return ELogVerbosity::Error;
        }
    }

    SNotificationItem::ECompletionState FErrorInfo::GetNotificationState() const
    {
        switch (GetErrorSeverity(Code))
        {
        case EErrorSeverity::Info:
            return SNotificationItem::CS_None;
        case EErrorSeverity::Warning:
            return SNotificationItem::CS_Pending;
        case EErrorSeverity::Error:
        case EErrorSeverity::Fatal:
            return SNotificationItem::CS_Fail;
        default:
            return SNotificationItem::CS_Fail;
        }
    }

    FString FErrorInfo::GetSuggestion() const
    {
        switch (Code)
        {
        case EErrorCode::InvalidCredentials:
            return TEXT("请检查SecretId和SecretKey是否正确，或在腾讯云控制台重新生成");

        case EErrorCode::ConfigSaveFailed:
            return TEXT("请检查磁盘空间是否充足，以及配置文件目录是否有写入权限");

        case EErrorCode::NetworkUnavailable:
            return TEXT("请检查网络连接，或确认防火墙是否阻止了程序访问网络");

        case EErrorCode::RequestTimeout:
            return TEXT("请稍后重试，或检查网络质量");

        case EErrorCode::ApiAuthenticationFailed:
            return TEXT("请确认SecretId和SecretKey是否有效，以及是否开启了相应的API服务");

        case EErrorCode::ApiRateLimitExceeded:
            return TEXT("请稍后再试，或升级API调用配额");

        case EErrorCode::FileNotFound:
            return TEXT("请检查文件是否被移动或删除");

        case EErrorCode::FileWriteFailed:
            return TEXT("请检查磁盘空间和文件写入权限");

        case EErrorCode::FileExtractFailed:
            return TEXT("请检查ZIP文件是否完整，或尝试手动解压");

        case EErrorCode::UnsupportedFormat:
            return TEXT("请使用FBX、OBJ、GLTF/GLB等支持的格式");

        case EErrorCode::ImportFailed:
            return TEXT("请检查文件格式是否正确，或尝试手动导入");

        default:
            return TEXT("请查看日志获取更多信息，或联系技术支持");
        }
    }
}

// 错误码到字符串的转换（用于序列化）
FString LexToString(HunYuanError::EErrorCode Code)
{
    return FString::FromInt(static_cast<int32>(Code));
}

bool LexTryParseString(HunYuanError::EErrorCode& OutCode, const TCHAR* Buffer)
{
    int32 Value = FCString::Atoi(Buffer);
    OutCode = static_cast<HunYuanError::EErrorCode>(Value);
    return true;
}

void LexFromString(HunYuanError::EErrorCode& OutValue, const TCHAR* Buffer)
{
    OutValue = static_cast<HunYuanError::EErrorCode>(FCString::Atoi(Buffer));
}