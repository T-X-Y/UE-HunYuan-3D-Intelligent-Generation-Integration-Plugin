#include "Download/ZipExtractor.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "HunYuanAI.h"
#include "Logging/HunYuanLogging.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include <fileapi.h>
#include "Windows/HideWindowsPlatformTypes.h"

// 支持的模型格式
static const TArray<FString> SupportedModelExtensions = {
    TEXT(".obj"), TEXT(".fbx"), TEXT(".glb"),
    TEXT(".gltf"), TEXT(".stl"), TEXT(".3ds"),
    TEXT(".dae"), TEXT(".blend"), TEXT(".max")
};

bool FZipExtractor::IsZipFile(const FString& FilePath)
{
    FString Extension = FPaths::GetExtension(FilePath).ToLower();
    return Extension == TEXT("zip");
}

FString FZipExtractor::GenerateExtractDestination(const FString& ZipFilePath)
{
    // 例如: /Downloads/abc.zip -> /Downloads/abc/
    return FPaths::GetPath(ZipFilePath) / FPaths::GetBaseFilename(ZipFilePath);
}

FString FZipExtractor::Find7ZipPath()
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    // 1. 引擎插件目录
    FString EnginePluginPath = FPaths::EngineDir() / TEXT("Plugins/7-Zip/7za.exe");
    if (PlatformFile.FileExists(*EnginePluginPath))
    {
        UE_LOG(LogHunYuanZip, Log, TEXT("Found 7-Zip in Engine plugins: %s"), *EnginePluginPath);
        return EnginePluginPath;
    }

    // 2. 系统安装的7-Zip（备选）
    TArray<FString> SystemPaths = {
        TEXT("C:\\Program Files\\7-Zip\\7z.exe"),
        TEXT("C:\\Program Files (x86)\\7-Zip\\7z.exe"),
        TEXT("D:\\Program Files\\7-Zip\\7z.exe"),
        TEXT("D:\\Program Files (x86)\\7-Zip\\7z.exe")
    };

    for (const FString& Path : SystemPaths)
    {
        if (PlatformFile.FileExists(*Path))
        {
            UE_LOG(LogHunYuanZip, Log, TEXT("Found 7-Zip in system: %s"), *Path);
            return Path;
        }
    }

    UE_LOG(LogHunYuanZip, Warning, TEXT("7-Zip not found"));
    return FString();
}

bool FZipExtractor::ExtractWith7Zip(const FString& ZipFilePath, const FString& DestDir)
{
    FString SevenZipPath = Find7ZipPath();

    if (SevenZipPath.IsEmpty())
    {
        UE_LOG(LogHunYuanZip, Error, TEXT("ExtractWith7Zip: 7-Zip not found"));
        return false;
    }

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    // 确保目标目录存在
    if (!PlatformFile.DirectoryExists(*DestDir))
    {
        PlatformFile.CreateDirectoryTree(*DestDir);
    }

    // 7z x [压缩文件] -o[目标目录] -y -aoa (自动覆盖)
    // x: 解压并保持目录结构
    // -o: 输出目录
    // -y: 所有提示都回答是
    // -aoa: 覆盖所有现有文件
    FString Params = FString::Printf(
        TEXT("x \"%s\" -o\"%s\" -y -aoa"),
        *ZipFilePath,
        *DestDir
    );

    UE_LOG(LogHunYuanZip, Log, TEXT("ExtractWith7Zip: %s %s"), *SevenZipPath, *Params);

    void* ReadPipe = nullptr;
    void* WritePipe = nullptr;

    // 创建进程
    FProcHandle Proc = FPlatformProcess::CreateProc(
        *SevenZipPath,
        *Params,
        false,  // bLaunchDetached
        true,   // bLaunchHidden
        true,   // bLaunchReallyHidden
        nullptr,
        0,
        nullptr,
        WritePipe,
        ReadPipe
    );

    if (!Proc.IsValid())
    {
        UE_LOG(LogHunYuanZip, Error, TEXT("ExtractWith7Zip: Failed to create process"));
        return false;
    }

    // 等待解压完成
    FPlatformProcess::WaitForProc(Proc);

    // 获取返回码
    int32 ReturnCode = 0;
    FPlatformProcess::GetProcReturnCode(Proc, &ReturnCode);
    FPlatformProcess::CloseProc(Proc);

    // 7-Zip 返回0表示成功
    bool bSuccess = (ReturnCode == 0);

    if (bSuccess)
    {
        UE_LOG(LogHunYuanZip, Log, TEXT("ExtractWith7Zip: Success"));

        // 验证解压结果
        TArray<FString> ExtractedFiles;
        PlatformFile.FindFilesRecursively(ExtractedFiles, *DestDir, TEXT("*"));
        UE_LOG(LogHunYuanZip, Log, TEXT("Extracted %d files to %s"), ExtractedFiles.Num(), *DestDir);
    }
    else
    {
        UE_LOG(LogHunYuanZip, Error, TEXT("ExtractWith7Zip: Failed with code %d"), ReturnCode);
    }

    return bSuccess;
}

bool FZipExtractor::Extract(const FString& ZipFilePath, FString& OutExtractedDir)
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    // 1. 标准化路径
    FString NormZipPath = FPaths::ConvertRelativePathToFull(ZipFilePath);
    NormZipPath = NormZipPath.Replace(TEXT("\\"), TEXT("/"));

    UE_LOG(LogHunYuanZip, Log, TEXT("Extract: Normalized zip path: %s"), *NormZipPath);

    // 2. 检查文件
    if (!PlatformFile.FileExists(*NormZipPath))
    {
        UE_LOG(LogHunYuanZip, Error, TEXT("Extract: File not found - %s"), *NormZipPath);
        return false;
    }

    if (!IsZipFile(NormZipPath))
    {
        UE_LOG(LogHunYuanZip, Error, TEXT("Extract: Not a ZIP file - %s"), *NormZipPath);
        return false;
    }

    // 3. 生成解压目录
    FString DestDir = GenerateExtractDestination(NormZipPath);
    DestDir = DestDir.Replace(TEXT("\\"), TEXT("/"));

    UE_LOG(LogHunYuanZip, Log, TEXT("Extract: Destination directory: %s"), *DestDir);

    // 4. 如果目录已存在，先删除（避免残留文件）
    if (PlatformFile.DirectoryExists(*DestDir))
    {
        UE_LOG(LogHunYuanZip, Log, TEXT("Extract: Removing existing directory %s"), *DestDir);
        PlatformFile.DeleteDirectoryRecursively(*DestDir);
    }

    // 5. 创建新目录
    if (!PlatformFile.CreateDirectoryTree(*DestDir))
    {
        UE_LOG(LogHunYuanZip, Error, TEXT("Extract: Failed to create directory %s"), *DestDir);
        return false;
    }

    // 6. 用7-Zip解压
    bool bExtracted = ExtractWith7Zip(NormZipPath, DestDir);

    if (!bExtracted)
    {
        UE_LOG(LogHunYuanZip, Error, TEXT("Extract: Failed to extract %s"), *NormZipPath);

        // 解压失败，删除空目录
        PlatformFile.DeleteDirectoryRecursively(*DestDir);
        return false;
    }

    // 7. 等待文件系统刷新
    UE_LOG(LogHunYuanZip, Log, TEXT("Extract: Waiting for file system to refresh..."));
    FPlatformProcess::Sleep(0.3f);

    // 8. 用多种方法验证解压结果
    UE_LOG(LogHunYuanZip, Log, TEXT("Extract: Verifying extracted files..."));

    // 方法1: 系统API
    TArray<FString> SystemFiles;
    FindFilesWithSystemAPI(DestDir, SystemFiles);
    UE_LOG(LogHunYuanZip, Log, TEXT("Extract: System API found %d files"), SystemFiles.Num());

    // 方法2: UE FindFiles
    TArray<FString> UEFiles;
    PlatformFile.FindFiles(UEFiles, *DestDir, TEXT("*"));
    UE_LOG(LogHunYuanZip, Log, TEXT("Extract: UE FindFiles found %d files"), UEFiles.Num());

    // 方法3: UE FindFilesRecursively
    TArray<FString> UERecursiveFiles;
    PlatformFile.FindFilesRecursively(UERecursiveFiles, *DestDir, TEXT("*"));
    UE_LOG(LogHunYuanZip, Log, TEXT("Extract: UE FindFilesRecursively found %d files"), UERecursiveFiles.Num());

    // 列出找到的文件（最多10个）
    int32 MaxLogCount = FMath::Min(10, SystemFiles.Num());
    for (int32 i = 0; i < MaxLogCount; i++)
    {
        UE_LOG(LogHunYuanZip, Log, TEXT("  File[%d]: %s"), i, *SystemFiles[i]);
    }
    if (SystemFiles.Num() > MaxLogCount)
    {
        UE_LOG(LogHunYuanZip, Log, TEXT("  ... and %d more files"), SystemFiles.Num() - MaxLogCount);
    }

    OutExtractedDir = DestDir;
    return true;
}

bool FZipExtractor::FindFilesWithSystemAPI(const FString& Directory, TArray<FString>& OutFiles)
{
    OutFiles.Empty();

    // 转换为Windows路径格式
    FString SearchPath = Directory / TEXT("*");
    SearchPath = SearchPath.Replace(TEXT("/"), TEXT("\\"));

    WIN32_FIND_DATA FindData;
    HANDLE hFind = FindFirstFile(*SearchPath, &FindData);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        UE_LOG(LogHunYuanZip, Warning, TEXT("FindFirstFile failed for: %s, error: %d"), *SearchPath, GetLastError());
        return false;
    }

    do
    {
        // 跳过 . 和 ..
        if (FCString::Strcmp(FindData.cFileName, TEXT(".")) == 0 ||
            FCString::Strcmp(FindData.cFileName, TEXT("..")) == 0)
        {
            continue;
        }

        // 如果是文件（不是目录）
        if (!(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            FString FullPath = Directory / FString(FindData.cFileName);
            FullPath = FullPath.Replace(TEXT("\\"), TEXT("/"));
            OutFiles.Add(FullPath);
            UE_LOG(LogHunYuanZip, Verbose, TEXT("Found file: %s"), *FullPath);
        }

    } while (FindNextFile(hFind, &FindData) != 0);

    FindClose(hFind);

    UE_LOG(LogHunYuanZip, Log, TEXT("FindFilesWithSystemAPI: Found %d files in %s"), OutFiles.Num(), *Directory);
    return true;
}

bool FZipExtractor::FindModelFileInDirectory(const FString& Directory, FString& OutModelFilePath)
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    if (!PlatformFile.DirectoryExists(*Directory))
    {
        UE_LOG(LogHunYuanZip, Warning, TEXT("FindModelFileInDirectory: Directory does not exist - %s"), *Directory);
        return false;
    }

    // 方法1: 用系统API直接读目录（最可靠）
    TArray<FString> AllFiles;
    if (FindFilesWithSystemAPI(Directory, AllFiles))
    {
        UE_LOG(LogHunYuanZip, Log, TEXT("FindModelFileInDirectory: System API found %d files"), AllFiles.Num());
    }

    // 方法2: 如果系统API没找到，用UE的FindFiles
    if (AllFiles.Num() == 0)
    {
        PlatformFile.FindFiles(AllFiles, *Directory, TEXT("*"));
        UE_LOG(LogHunYuanZip, Log, TEXT("FindModelFileInDirectory: UE FindFiles found %d files"), AllFiles.Num());
    }

    // 方法3: 递归查找
    if (AllFiles.Num() == 0)
    {
        PlatformFile.FindFilesRecursively(AllFiles, *Directory, TEXT("*"));
        UE_LOG(LogHunYuanZip, Log, TEXT("FindModelFileInDirectory: UE FindFilesRecursively found %d files"), AllFiles.Num());
    }

    // 列出所有找到的文件
    for (const FString& File : AllFiles)
    {
        UE_LOG(LogHunYuanZip, Log, TEXT("  File: %s"), *File);
    }

    // 支持的模型格式
    TArray<FString> ModelExtensions = {
        TEXT(".obj"), TEXT(".fbx"), TEXT(".glb"),
        TEXT(".gltf"), TEXT(".stl"), TEXT(".3ds"),
        TEXT(".dae"), TEXT(".blend"), TEXT(".max")
    };

    // 查找模型文件
    for (const FString& Ext : ModelExtensions)
    {
        for (const FString& File : AllFiles)
        {
            if (File.EndsWith(Ext, ESearchCase::IgnoreCase))
            {
                OutModelFilePath = File;
                UE_LOG(LogHunYuanZip, Log, TEXT("FindModelFileInDirectory: Found model file - %s"), *File);
                return true;
            }
        }
    }

    UE_LOG(LogHunYuanZip, Warning, TEXT("FindModelFileInDirectory: No model file found in %s"), *Directory);
    return false;
}

bool FZipExtractor::ExtractAndFindModel(const FString& ZipFilePath, FString& OutModelFilePath)
{
    FString ExtractedDir;
    if (!Extract(ZipFilePath, ExtractedDir))
    {
        return false;
    }

    return FindModelFileInDirectory(ExtractedDir, OutModelFilePath);
}