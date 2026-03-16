#include "Config/HunYuanConfigManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "HunYuanAI.h"
#include "Logging/HunYuanLogging.h"

TSharedPtr<FHunYuanConfigManager> FHunYuanConfigManager::Instance = nullptr;
FCriticalSection FHunYuanConfigManager::InstanceCriticalSection;

TSharedPtr<FHunYuanConfigManager> FHunYuanConfigManager::Get()
{
    FScopeLock Lock(&InstanceCriticalSection);
    if (!Instance.IsValid())
    {
        Instance = MakeShareable(new FHunYuanConfigManager());
        Instance->LoadConfig();
    }
    return Instance;
}

void FHunYuanConfigManager::Shutdown()
{
    FScopeLock Lock(&InstanceCriticalSection);
    if (Instance.IsValid())
    {
        Instance->SaveConfig();
        Instance.Reset();
    }
}

FHunYuanConfigManager::FHunYuanConfigManager()
    : bLoaded(false)
{
    ConfigFilePath = HunYuanConfig::FConfigData::GetDefaultConfigFile();
    Config.DownloadDirectory = HunYuanConfig::FConfigData::GetDefaultDownloadDirectory();
}

FHunYuanConfigManager::~FHunYuanConfigManager()
{
    SaveConfig();
}

void FHunYuanConfigManager::LoadConfig()
{
    FScopeLock Lock(&ConfigCriticalSection);

    EnsureConfigFileExists();
    Config.LoadFromConfig(ConfigFilePath);

    // 确保下载目录存在
    EnsureDirectoryExists(Config.DownloadDirectory);

    bLoaded = true;

    UE_LOG(LogHunYuanConfig, Log, TEXT("Config loaded from %s"), *ConfigFilePath);
}

void FHunYuanConfigManager::SaveConfig()
{
    FScopeLock Lock(&ConfigCriticalSection);

    EnsureConfigFileExists();
    Config.SaveToConfig(ConfigFilePath);

    UE_LOG(LogHunYuanConfig, Log, TEXT("Config saved to %s"), *ConfigFilePath);
}

HunYuanConfig::FConfigData FHunYuanConfigManager::GetConfig() const
{
    FScopeLock Lock(&ConfigCriticalSection);
    return Config;
}

void FHunYuanConfigManager::UpdateConfig(const HunYuanConfig::FConfigData& NewConfig)
{
    {
        FScopeLock Lock(&ConfigCriticalSection);
        Config = NewConfig;
        SaveConfig();
    }

    // 触发回调（在游戏线程）
    AsyncTask(ENamedThreads::GameThread, [WeakThis = AsWeak(), NewConfig]()
        {
            auto SharedThis = WeakThis.Pin();
            if (SharedThis.IsValid())
            {
                SharedThis->OnConfigChanged.Broadcast(NewConfig);
            }
        });
}

void FHunYuanConfigManager::UpdateCredentials(const FString& SecretId, const FString& SecretKey, bool bRemember)
{
    {
        FScopeLock Lock(&ConfigCriticalSection);
        Config.SecretId = SecretId;
        Config.SecretKey = SecretKey;
        Config.bRememberPassword = bRemember;
        SaveConfig();
    }

    // 触发回调
    auto CurrentConfig = GetConfig();
    AsyncTask(ENamedThreads::GameThread, [WeakThis = AsWeak(), CurrentConfig]()
        {
            auto SharedThis = WeakThis.Pin();
            if (SharedThis.IsValid())
            {
                SharedThis->OnConfigChanged.Broadcast(CurrentConfig);
            }
        });
}

void FHunYuanConfigManager::EnsureConfigFileExists()
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    FString ConfigDir = FPaths::GetPath(ConfigFilePath);

    if (!PlatformFile.DirectoryExists(*ConfigDir))
    {
        PlatformFile.CreateDirectoryTree(*ConfigDir);
    }
}

void FHunYuanConfigManager::EnsureDirectoryExists(const FString& DirPath)
{
    if (DirPath.IsEmpty()) return;

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*DirPath))
    {
        PlatformFile.CreateDirectoryTree(*DirPath);
    }
}

FString FHunYuanConfigManager::GetOrCreateDownloadDirectory()
{
    FScopeLock Lock(&ConfigCriticalSection);
    EnsureDirectoryExists(Config.DownloadDirectory);
    return Config.DownloadDirectory;
}