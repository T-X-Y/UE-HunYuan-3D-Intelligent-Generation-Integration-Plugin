#include "Logging/HunYuanLogging.h"

// 定义所有日志类别
DEFINE_LOG_CATEGORY(LogHunYuanAI);
DEFINE_LOG_CATEGORY(LogHunYuanAPI);
DEFINE_LOG_CATEGORY(LogHunYuanConfig);
DEFINE_LOG_CATEGORY(LogHunYuanDownload);
DEFINE_LOG_CATEGORY(LogHunYuanImport);
DEFINE_LOG_CATEGORY(LogHunYuanUI);
DEFINE_LOG_CATEGORY(LogHunYuanZip);
DEFINE_LOG_CATEGORY(LogHunYuanError);

// 可选：添加日志初始化函数（如果需要）
void InitializeHunYuanLogging()
{
    // 可以在这里设置日志级别
    // 例如：在开发版本中设置更详细的日志
#if !UE_BUILD_SHIPPING
    // 设置所有HunYuan相关的日志级别为Verbose
    FLogCategoryBase* Categories[] = {
        &LogHunYuanAI,
        &LogHunYuanAPI,
        &LogHunYuanConfig,
        &LogHunYuanDownload,
        &LogHunYuanImport,
        &LogHunYuanUI,
        &LogHunYuanZip,
        &LogHunYuanError
    };

    for (FLogCategoryBase* Category : Categories)
    {
        if (Category)
        {
            // 设置日志级别为Verbose，但不超过设置的级别
            // Category->SetVerbosity(ELogVerbosity::Verbose);
        }
    }
#endif

    UE_LOG(LogHunYuanAI, Log, TEXT("HunYuanAI Logging System Initialized"));
}