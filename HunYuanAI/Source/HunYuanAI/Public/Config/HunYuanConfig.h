#pragma once

#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Base64.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

// 格式偏好枚举（与之前定义的保持一致）
UENUM()
enum class EConfigFormatPreference : uint8
{
    Default = 0 UMETA(DisplayName = "默认 (OBJ+GLB)"),
    GLB = 1 UMETA(DisplayName = "GLB格式"),
    OBJ = 2 UMETA(DisplayName = "OBJ格式 (ZIP)"),
    STL = 3 UMETA(DisplayName = "STL格式 (3D打印)"),
    USDZ = 4 UMETA(DisplayName = "USDZ格式 (AR)"),
    FBX = 5 UMETA(DisplayName = "FBX格式 (动画)")
};

namespace HunYuanConfig
{
    // 配置数据结构
    struct FConfigData
    {
        FString SecretId;
        FString SecretKey;
        bool bRememberPassword = false;
        bool bAutoImport = false;
        bool bShowPreviewAfterDownload = true;
        FString DownloadDirectory;
        FString Region = TEXT("ap-guangzhou");
        FString LastUsedModel = TEXT("3.0");

        // 格式偏好（使用枚举）
        EConfigFormatPreference LastUsedFormat = EConfigFormatPreference::Default;

        // 构造函数
        FConfigData();

        // 序列化到配置文件
        void SaveToConfig(const FString& ConfigFile) const;

        // 从配置文件加载
        void LoadFromConfig(const FString& ConfigFile);

        // 清除凭证
        void ClearCredentials();

        // 是否有有效的凭证
        bool HasValidCredentials() const;

        // 获取默认配置文件路径
        static FString GetDefaultConfigFile();

        // 获取默认下载目录
        static FString GetDefaultDownloadDirectory();

        // 获取默认导入路径
        static FString GetDefaultImportPath();

        // 验证并修复下载目录
        void ValidateAndFixDownloadDirectory();

        // 转换为JSON
        TSharedPtr<FJsonObject> ToJson() const;

        // 从JSON加载
        bool FromJson(const TSharedPtr<FJsonObject>& Json);

        // 获取格式名称
        FString GetFormatName() const;

        // 获取格式描述
        FString GetFormatDescription() const;

    private:
        // 简单的字符串混淆
        static FString ObfuscateString(const FString& Input);
        static FString DeobfuscateString(const FString& Input);
    };

    // ==================== 全局辅助函数声明 ====================

    // 确保目录存在
    bool EnsureDirectoryExists(const FString& DirectoryPath);

    // 确保文件所在目录存在
    bool EnsureFileDirectoryExists(const FString& FilePath);

    // 清理文件名（移除非法字符）
    FString SanitizeFileName(const FString& FileName);

    // 生成唯一文件名
    FString GenerateUniqueFileName(const FString& BaseName, const FString& Extension);

    // 从旧配置迁移
    bool MigrateFromOldConfig(const FString& OldConfigFile, FConfigData& OutConfig);

    // 验证SecretId
    FString ValidateSecretId(const FString& SecretId);

    // 验证SecretKey
    FString ValidateSecretKey(const FString& SecretKey);

    // 验证下载目录
    FString ValidateDownloadDirectory(const FString& Directory);

    // 格式辅助函数（新增）
    FString GetFormatDisplayName(EConfigFormatPreference Format);
    FString GetFormatDescription(EConfigFormatPreference Format);
    EConfigFormatPreference StringToFormat(const FString& FormatStr);
    FString FormatToString(EConfigFormatPreference Format);
}