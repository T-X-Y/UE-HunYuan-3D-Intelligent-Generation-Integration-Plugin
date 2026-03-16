#include "Config/HunYuanConfig.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HunYuanAI.h"
#include "Logging/HunYuanLogging.h"

namespace HunYuanConfig
{
    // ==================== 构造函数 ====================
    FConfigData::FConfigData()
        : bRememberPassword(false)
        , bAutoImport(true)
        , bShowPreviewAfterDownload(true)
        , Region(TEXT("ap-guangzhou"))
        , LastUsedModel(TEXT("hunyuan-lite"))
        , DownloadDirectory(GetDefaultDownloadDirectory())
    {
    }

    // ==================== FConfigData 实现 ====================

    void FConfigData::SaveToConfig(const FString& ConfigFile) const
    {
        // 确保目录存在
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        FString ConfigDir = FPaths::GetPath(ConfigFile);
        if (!PlatformFile.DirectoryExists(*ConfigDir))
        {
            PlatformFile.CreateDirectoryTree(*ConfigDir);
        }

        // 保存基本配置
        GConfig->SetBool(TEXT("HunYuanAI"), TEXT("RememberPassword"), bRememberPassword, ConfigFile);
        GConfig->SetBool(TEXT("HunYuanAI"), TEXT("AutoImport"), bAutoImport, ConfigFile);
        GConfig->SetBool(TEXT("HunYuanAI"), TEXT("ShowPreviewAfterDownload"), bShowPreviewAfterDownload, ConfigFile);
        GConfig->SetString(TEXT("HunYuanAI"), TEXT("Region"), *Region, ConfigFile);
        GConfig->SetString(TEXT("HunYuanAI"), TEXT("LastUsedModel"), *LastUsedModel, ConfigFile);

        if (!DownloadDirectory.IsEmpty())
        {
            GConfig->SetString(TEXT("HunYuanAI"), TEXT("DownloadDirectory"), *DownloadDirectory, ConfigFile);
        }

        // 根据记住密码选项保存凭证
        if (bRememberPassword)
        {
            // 简单的混淆处理，避免明文存储
            FString ObfuscatedId = ObfuscateString(SecretId);
            FString ObfuscatedKey = ObfuscateString(SecretKey);

            GConfig->SetString(TEXT("HunYuanAI"), TEXT("SecretId"), *ObfuscatedId, ConfigFile);
            GConfig->SetString(TEXT("HunYuanAI"), TEXT("SecretKey"), *ObfuscatedKey, ConfigFile);

            UE_LOG(LogHunYuanConfig, Log, TEXT("Credentials saved with obfuscation"));
        }
        else
        {
            // 不记住密码时清除保存的凭证
            GConfig->SetString(TEXT("HunYuanAI"), TEXT("SecretId"), TEXT(""), ConfigFile);
            GConfig->SetString(TEXT("HunYuanAI"), TEXT("SecretKey"), TEXT(""), ConfigFile);

            UE_LOG(LogHunYuanConfig, Log, TEXT("Credentials cleared from config"));
        }

        GConfig->Flush(false, ConfigFile);

        UE_LOG(LogHunYuanConfig, Log, TEXT("Config saved to %s"), *ConfigFile);
    }

    void FConfigData::LoadFromConfig(const FString& ConfigFile)
    {
        if (!FPaths::FileExists(ConfigFile))
        {
            UE_LOG(LogHunYuanConfig, Log, TEXT("Config file not found: %s, using defaults"), *ConfigFile);
            return;
        }

        // 加载基本配置
        GConfig->GetBool(TEXT("HunYuanAI"), TEXT("RememberPassword"), bRememberPassword, ConfigFile);
        GConfig->GetBool(TEXT("HunYuanAI"), TEXT("AutoImport"), bAutoImport, ConfigFile);
        GConfig->GetBool(TEXT("HunYuanAI"), TEXT("ShowPreviewAfterDownload"), bShowPreviewAfterDownload, ConfigFile);
        GConfig->GetString(TEXT("HunYuanAI"), TEXT("Region"), Region, ConfigFile);
        GConfig->GetString(TEXT("HunYuanAI"), TEXT("LastUsedModel"), LastUsedModel, ConfigFile);
        GConfig->GetString(TEXT("HunYuanAI"), TEXT("DownloadDirectory"), DownloadDirectory, ConfigFile);

        // 加载凭证（如果记住密码）
        if (bRememberPassword)
        {
            FString ObfuscatedId, ObfuscatedKey;
            GConfig->GetString(TEXT("HunYuanAI"), TEXT("SecretId"), ObfuscatedId, ConfigFile);
            GConfig->GetString(TEXT("HunYuanAI"), TEXT("SecretKey"), ObfuscatedKey, ConfigFile);

            // 反混淆
            SecretId = DeobfuscateString(ObfuscatedId);
            SecretKey = DeobfuscateString(ObfuscatedKey);

            UE_LOG(LogHunYuanConfig, Log, TEXT("Credentials loaded from config"));
        }

        // 验证下载目录
        ValidateAndFixDownloadDirectory();
    }

    void FConfigData::ClearCredentials()
    {
        SecretId.Empty();
        SecretKey.Empty();
        bRememberPassword = false;

        UE_LOG(LogHunYuanConfig, Log, TEXT("Credentials cleared"));
    }

    bool FConfigData::HasValidCredentials() const
    {
        return !SecretId.IsEmpty() && !SecretKey.IsEmpty() &&
            SecretId.Len() > 10 && SecretKey.Len() > 10;
    }

    void FConfigData::ValidateAndFixDownloadDirectory()
    {
        if (DownloadDirectory.IsEmpty())
        {
            DownloadDirectory = GetDefaultDownloadDirectory();
        }

        // 确保路径格式正确
        DownloadDirectory = FPaths::ConvertRelativePathToFull(DownloadDirectory);

        // 确保以分隔符结尾
        if (!DownloadDirectory.EndsWith(TEXT("/")) && !DownloadDirectory.EndsWith(TEXT("\\")))
        {
            DownloadDirectory += TEXT("/");
        }
    }

    FString FConfigData::GetDefaultConfigFile()
    {
        return FPaths::ProjectSavedDir() / TEXT("HunYuanAI/Config.ini");
    }

    FString FConfigData::GetDefaultDownloadDirectory()
    {
        return FPaths::ProjectSavedDir() / TEXT("HunYuanAI/Downloads/");
    }

    FString FConfigData::GetDefaultImportPath()
    {
        return TEXT("/Game/HunyuanImports/");
    }

    // 简单的字符串混淆（非加密，仅防止明文存储）
    FString FConfigData::ObfuscateString(const FString& Input)
    {
        if (Input.IsEmpty())
        {
            return Input;
        }

        TArray<uint8> Bytes;
        FTCHARToUTF8 Converter(*Input);
        Bytes.Append(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());

        // 简单的XOR混淆
        const uint8 Key = 0x5A;
        for (int32 i = 0; i < Bytes.Num(); i++)
        {
            Bytes[i] ^= Key;
        }

        // 转换为Base64
        return FBase64::Encode(Bytes);
    }

    FString FConfigData::DeobfuscateString(const FString& Input)
    {
        if (Input.IsEmpty())
        {
            return Input;
        }

        TArray<uint8> Bytes;
        if (!FBase64::Decode(Input, Bytes))
        {
            return Input; // 解码失败，返回原字符串
        }

        // XOR反混淆
        const uint8 Key = 0x5A;
        for (int32 i = 0; i < Bytes.Num(); i++)
        {
            Bytes[i] ^= Key;
        }

        // 转换为UTF-8字符串
        Bytes.Add(0); // 添加终止符
        return UTF8_TO_TCHAR(reinterpret_cast<const char*>(Bytes.GetData()));
    }

    // ==================== 全局辅助函数 ====================

    bool EnsureDirectoryExists(const FString& DirectoryPath)
    {
        if (DirectoryPath.IsEmpty())
        {
            return false;
        }

        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

        if (!PlatformFile.DirectoryExists(*DirectoryPath))
        {
            return PlatformFile.CreateDirectoryTree(*DirectoryPath);
        }

        return true;
    }

    bool EnsureFileDirectoryExists(const FString& FilePath)
    {
        FString DirectoryPath = FPaths::GetPath(FilePath);
        return EnsureDirectoryExists(DirectoryPath);
    }

    FString SanitizeFileName(const FString& FileName)
    {
        FString Result = FileName;

        // 替换Windows文件名中不允许的字符
        Result = Result.Replace(TEXT("\\"), TEXT("_"))
            .Replace(TEXT("/"), TEXT("_"))
            .Replace(TEXT(":"), TEXT("_"))
            .Replace(TEXT("*"), TEXT("_"))
            .Replace(TEXT("?"), TEXT("_"))
            .Replace(TEXT("\""), TEXT("_"))
            .Replace(TEXT("<"), TEXT("_"))
            .Replace(TEXT(">"), TEXT("_"))
            .Replace(TEXT("|"), TEXT("_"));

        // 限制长度
        const int32 MaxFileNameLength = 64;
        if (Result.Len() > MaxFileNameLength)
        {
            Result = Result.Left(MaxFileNameLength);
        }

        return Result;
    }

    FString GenerateUniqueFileName(const FString& BaseName, const FString& Extension)
    {
        FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
        FString SafeBaseName = SanitizeFileName(BaseName);

        return FString::Printf(TEXT("%s_%s%s"), *SafeBaseName, *Timestamp, *Extension);
    }

    // ==================== 配置迁移 ====================

    bool MigrateFromOldConfig(const FString& OldConfigFile, FConfigData& OutConfig)
    {
        if (!FPaths::FileExists(OldConfigFile))
        {
            return false;
        }

        UE_LOG(LogHunYuanConfig, Log, TEXT("Migrating config from %s"), *OldConfigFile);

        // 加载旧配置
        GConfig->GetString(TEXT("HunYuanAI"), TEXT("SecretId"), OutConfig.SecretId, OldConfigFile);
        GConfig->GetString(TEXT("HunYuanAI"), TEXT("SecretKey"), OutConfig.SecretKey, OldConfigFile);
        GConfig->GetBool(TEXT("HunYuanAI"), TEXT("RememberPassword"), OutConfig.bRememberPassword, OldConfigFile);
        GConfig->GetBool(TEXT("HunYuanAI"), TEXT("AutoImport"), OutConfig.bAutoImport, OldConfigFile);
        GConfig->GetBool(TEXT("HunYuanAI"), TEXT("ShowPreviewAfterDownload"), OutConfig.bShowPreviewAfterDownload, OldConfigFile);
        GConfig->GetString(TEXT("HunYuanAI"), TEXT("DownloadDirectory"), OutConfig.DownloadDirectory, OldConfigFile);

        // 验证数据
        OutConfig.ValidateAndFixDownloadDirectory();

        UE_LOG(LogHunYuanConfig, Log, TEXT("Config migration completed"));
        return true;
    }

    // ==================== 配置验证 ====================

    FString ValidateSecretId(const FString& SecretId)
    {
        if (SecretId.IsEmpty())
        {
            return TEXT("SecretId不能为空");
        }

        if (SecretId.Len() < 10)
        {
            return TEXT("SecretId长度不正确");
        }

        // 检查格式（腾讯云SecretId通常以AKID开头）
        if (!SecretId.StartsWith(TEXT("AKID")) && !SecretId.StartsWith(TEXT("SKID")))
        {
            return TEXT("SecretId格式不正确，应以AKID或SKID开头");
        }

        return FString();
    }

    FString ValidateSecretKey(const FString& SecretKey)
    {
        if (SecretKey.IsEmpty())
        {
            return TEXT("SecretKey不能为空");
        }

        if (SecretKey.Len() < 10)
        {
            return TEXT("SecretKey长度不正确");
        }

        return FString();
    }

    FString ValidateDownloadDirectory(const FString& Directory)
    {
        if (Directory.IsEmpty())
        {
            return TEXT("下载目录不能为空");
        }

        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

        // 检查目录是否存在（如果不存在，尝试创建）
        if (!PlatformFile.DirectoryExists(*Directory))
        {
            if (!PlatformFile.CreateDirectoryTree(*Directory))
            {
                return TEXT("无法创建下载目录，请检查权限");
            }
        }

        // 检查写入权限（尝试创建临时文件）
        FString TestFile = Directory / TEXT(".write_test.tmp");
        if (!FFileHelper::SaveStringToFile(TEXT("test"), *TestFile))
        {
            return TEXT("下载目录无写入权限");
        }

        // 清理测试文件
        IFileManager::Get().Delete(*TestFile);

        return FString();
    }

    // ==================== 序列化 ====================

    TSharedPtr<FJsonObject> FConfigData::ToJson() const
    {
        TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject());

        Json->SetStringField(TEXT("Region"), Region);
        Json->SetStringField(TEXT("LastUsedModel"), LastUsedModel);
        Json->SetStringField(TEXT("DownloadDirectory"), DownloadDirectory);
        Json->SetBoolField(TEXT("AutoImport"), bAutoImport);
        Json->SetBoolField(TEXT("ShowPreviewAfterDownload"), bShowPreviewAfterDownload);

        // 不保存凭证到JSON

        return Json;
    }

    bool FConfigData::FromJson(const TSharedPtr<FJsonObject>& Json)
    {
        if (!Json.IsValid())
        {
            return false;
        }

        Json->TryGetStringField(TEXT("Region"), Region);
        Json->TryGetStringField(TEXT("LastUsedModel"), LastUsedModel);
        Json->TryGetStringField(TEXT("DownloadDirectory"), DownloadDirectory);
        Json->TryGetBoolField(TEXT("AutoImport"), bAutoImport);
        Json->TryGetBoolField(TEXT("ShowPreviewAfterDownload"), bShowPreviewAfterDownload);

        ValidateAndFixDownloadDirectory();

        return true;
    }
}