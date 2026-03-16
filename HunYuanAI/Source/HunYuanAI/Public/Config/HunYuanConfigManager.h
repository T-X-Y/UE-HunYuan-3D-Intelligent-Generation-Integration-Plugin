#pragma once

#include "CoreMinimal.h"
#include "HunYuanConfig.h"
#include "HAL/ThreadSafeBool.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnHunYuanConfigChanged, const HunYuanConfig::FConfigData&);

class HUNYUANAI_API FHunYuanConfigManager : public TSharedFromThis<FHunYuanConfigManager>
{
public:
    static TSharedPtr<FHunYuanConfigManager> Get();
    static void Shutdown();

    ~FHunYuanConfigManager();

    // 加载配置
    void LoadConfig();

    // 保存配置
    void SaveConfig();

    // 获取配置（线程安全）
    HunYuanConfig::FConfigData GetConfig() const;

    // 更新配置（线程安全）
    void UpdateConfig(const HunYuanConfig::FConfigData& NewConfig);

    // 更新凭证
    void UpdateCredentials(const FString& SecretId, const FString& SecretKey, bool bRemember);

    // 配置变更事件
    FOnHunYuanConfigChanged OnConfigChanged;

    // 获取配置文件路径
    FString GetConfigFilePath() const { return ConfigFilePath; }

    // 获取下载目录（确保存在）
    FString GetOrCreateDownloadDirectory();

private:
    FHunYuanConfigManager();

    void EnsureConfigFileExists();
    void EnsureDirectoryExists(const FString& DirPath);

private:
    static TSharedPtr<FHunYuanConfigManager> Instance;
    static FCriticalSection InstanceCriticalSection;

    mutable FCriticalSection ConfigCriticalSection;
    HunYuanConfig::FConfigData Config;
    FString ConfigFilePath;
    FThreadSafeBool bLoaded;
};