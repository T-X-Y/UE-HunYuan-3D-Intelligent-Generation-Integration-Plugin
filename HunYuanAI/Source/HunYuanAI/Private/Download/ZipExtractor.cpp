#include "Download/ZipExtractor.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "HunYuanAI.h"
#include "Logging/HunYuanLogging.h"

bool FZipExtractor::ExtractModelFromZip(const FString& ZipFilePath, FString& OutModelPath)
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    if (!PlatformFile.FileExists(*ZipFilePath))
    {
        UE_LOG(LogHunYuanZip, Error, TEXT("ZIP file does not exist: %s"), *ZipFilePath);
        return false;
    }

    if (!IsZipFile(ZipFilePath))
    {
        UE_LOG(LogHunYuanZip, Warning, TEXT("File is not a ZIP file: %s"), *ZipFilePath);
        return false;
    }

    if (!IsZipFile(ZipFilePath))
    {
        UE_LOG(LogHunYuanZip, Warning, TEXT("File is not a ZIP file: %s"), *ZipFilePath);
        return false;
    }

    FString DestDir = GetExtractDestination(ZipFilePath);

    if (!UnzipToDirectory(ZipFilePath, DestDir))
    {
        UE_LOG(LogHunYuanZip, Error, TEXT("Failed to extract ZIP file: %s"), *ZipFilePath);
        return false;
    }

    OutModelPath = DestDir;
    // 验证目录中是否有模型文件
    TArray<FString> FoundFiles;
    PlatformFile.FindFiles(FoundFiles, *DestDir, TEXT("*.obj"));

    if (FoundFiles.Num() > 0)
    {
        UE_LOG(LogHunYuanZip, Log, TEXT("Successfully extracted model directory: %s"), *DestDir);
        UE_LOG(LogHunYuanZip, Log, TEXT("Found OBJ file: %s"), *FoundFiles[0]);
        return true;
    }
    else
    {
        UE_LOG(LogHunYuanZip, Warning, TEXT("No OBJ file found in extracted directory: %s"), *DestDir);
        // 仍然返回 true，因为目录存在，可能在其他子目录中
        return true;
    }
}

bool FZipExtractor::IsZipFile(const FString& FilePath)
{
    FString Extension = FPaths::GetExtension(FilePath).ToLower();
    return Extension == TEXT("zip");
}

FString FZipExtractor::GetExtractDestination(const FString& ZipFilePath)
{
    return FPaths::GetPath(ZipFilePath) / FPaths::GetBaseFilename(ZipFilePath);
}

bool FZipExtractor::UnzipToDirectory(const FString& ZipFilePath, const FString& DestDir)
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    if (!PlatformFile.DirectoryExists(*DestDir))
    {
        PlatformFile.CreateDirectoryTree(*DestDir);
    }

#if PLATFORM_WINDOWS
    return ExecutePowerShellUnzip(ZipFilePath, DestDir);
#else
    UE_LOG(LogHunYuanZip, Error, TEXT("Unzip not supported on this platform"));
    return false;
#endif
}

bool FZipExtractor::ExecutePowerShellUnzip(const FString& ZipFilePath, const FString& DestDir)
{
    FString Params = FString::Printf(
        TEXT("-Command \"Expand-Archive -Path '%s' -DestinationPath '%s' -Force\""),
        *ZipFilePath.Replace(TEXT("'"), TEXT("''")),
        *DestDir.Replace(TEXT("'"), TEXT("''"))
    );

    void* ReadPipe = nullptr;
    void* WritePipe = nullptr;

    FProcHandle Proc = FPlatformProcess::CreateProc(
        TEXT("powershell.exe"),
        *Params,
        false,
        true,
        true,
        nullptr,
        0,
        nullptr,
        WritePipe,
        ReadPipe
    );

    if (!Proc.IsValid())
    {
        UE_LOG(LogHunYuanZip, Error, TEXT("Failed to create unzip process"));
        return false;
    }

    FPlatformProcess::WaitForProc(Proc);
    int32 ReturnCode = 0;
    FPlatformProcess::GetProcReturnCode(Proc, &ReturnCode);
    FPlatformProcess::CloseProc(Proc);

    return (ReturnCode == 0);
}

bool FZipExtractor::FindModelFileInDirectory(const FString& Directory, FString& OutModelPath)
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    // 按优先级排序的模型格式
    TArray<FString> ModelExtensions = {
        TEXT(".obj"), TEXT(".gltf"), TEXT(".fbx"),
        TEXT(".glb"), TEXT(".stl"), TEXT(".3ds")
    };

    TMap<FString, TArray<FString>> FoundFilesByExt;

    // 递归查找所有文件
    TArray<FString> AllFiles;
    PlatformFile.FindFilesRecursively(AllFiles, *Directory, TEXT("*"));

    for (const FString& File : AllFiles)
    {
        FString Extension = FPaths::GetExtension(File).ToLower();
        FString FileName = FPaths::GetCleanFilename(File).ToLower();

        // 检查是否是支持的模型格式
        if (ModelExtensions.Contains(Extension))
        {
            // 过滤掉预览图、缩略图等
            if (FileName.Contains(TEXT("preview")) ||
                FileName.Contains(TEXT("thumbnail")) ||
                FileName.Contains(TEXT("icon")))
            {
                continue;
            }

            FoundFilesByExt.FindOrAdd(Extension).Add(File);
        }
    }

    // 按优先级返回第一个找到的文件
    for (const FString& Ext : ModelExtensions)
    {
        if (FoundFilesByExt.Contains(Ext) && FoundFilesByExt[Ext].Num() > 0)
        {
            OutModelPath = FoundFilesByExt[Ext][0];
            return true;
        }
    }

    return false;
}