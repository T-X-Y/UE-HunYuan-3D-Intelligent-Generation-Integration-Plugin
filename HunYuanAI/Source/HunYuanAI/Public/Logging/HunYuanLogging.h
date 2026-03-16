#pragma once

#include "CoreMinimal.h"

// 声明所有插件级别的日志类别
DECLARE_LOG_CATEGORY_EXTERN(LogHunYuanAI, Log, All);           // 主日志
DECLARE_LOG_CATEGORY_EXTERN(LogHunYuanAPI, Log, All);          // API相关
DECLARE_LOG_CATEGORY_EXTERN(LogHunYuanConfig, Log, All);       // 配置相关
DECLARE_LOG_CATEGORY_EXTERN(LogHunYuanDownload, Log, All);     // 下载相关
DECLARE_LOG_CATEGORY_EXTERN(LogHunYuanImport, Log, All);       // 导入相关
DECLARE_LOG_CATEGORY_EXTERN(LogHunYuanUI, Log, All);           // UI相关
DECLARE_LOG_CATEGORY_EXTERN(LogHunYuanZip, Log, All);          // ZIP解压相关
DECLARE_LOG_CATEGORY_EXTERN(LogHunYuanError, Log, All);        // 错误处理相关

// 可选的日志辅助宏（方便统一控制日志输出）
#if UE_BUILD_SHIPPING
#define HUNYUAN_LOG(Verbosity, Format, ...) 
#else
#define HUNYUAN_LOG(Verbosity, Format, ...) UE_LOG(LogHunYuanAI, Verbosity, TEXT(Format), ##__VA_ARGS__)
#endif

// 模块特定的日志宏
#define HUNYUAN_API_LOG(Verbosity, Format, ...) UE_LOG(LogHunYuanAPI, Verbosity, TEXT(Format), ##__VA_ARGS__)
#define HUNYUAN_CONFIG_LOG(Verbosity, Format, ...) UE_LOG(LogHunYuanConfig, Verbosity, TEXT(Format), ##__VA_ARGS__)
#define HUNYUAN_DOWNLOAD_LOG(Verbosity, Format, ...) UE_LOG(LogHunYuanDownload, Verbosity, TEXT(Format), ##__VA_ARGS__)
#define HUNYUAN_IMPORT_LOG(Verbosity, Format, ...) UE_LOG(LogHunYuanImport, Verbosity, TEXT(Format), ##__VA_ARGS__)
#define HUNYUAN_UI_LOG(Verbosity, Format, ...) UE_LOG(LogHunYuanUI, Verbosity, TEXT(Format), ##__VA_ARGS__)
#define HUNYUAN_ZIP_LOG(Verbosity, Format, ...) UE_LOG(LogHunYuanZip, Verbosity, TEXT(Format), ##__VA_ARGS__)
#define HUNYUAN_ERROR_LOG(Verbosity, Format, ...) UE_LOG(LogHunYuanError, Verbosity, TEXT(Format), ##__VA_ARGS__)

// 性能追踪宏（调试用）
#if !UE_BUILD_SHIPPING
#define SCOPED_HUNYUAN_TIMER(Name) DECLARE_SCOPE_CYCLE_COUNTER(TEXT(Name), STAT_##Name, STATGROUP_HunYuan)
#else
#define SCOPED_HUNYUAN_TIMER(Name)
#endif