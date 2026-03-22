// ObjZipDownloadHandler.cpp
#include "Download/ObjZipDownloadHandler.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Logging/HunYuanLogging.h"

bool FObjZipDownloadHandler::CanHandle(const FString& URL) const
{
    FString LowerURL = URL.ToLower();
    return LowerURL.Contains(TEXT("obj")) ||
        LowerURL.EndsWith(TEXT(".zip")) ||
        LowerURL.Contains(TEXT(".zip?")) ||
        LowerURL.EndsWith(TEXT(".obj")) ||
        LowerURL.Contains(TEXT(".obj?"));
}

bool FObjZipDownloadHandler::ProcessDownloadedFile(
    const FString& FilePath,
    const FString& JobId,
    FModelInfo& OutModelInfo)
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    if (!PlatformFile.FileExists(*FilePath))
    {
        UE_LOG(LogHunYuanDownload, Error, TEXT("FObjZipDownloadHandler: File not found - %s"), *FilePath);
        return false;
    }

    FString FinalPath = FilePath;
    FString FileExtension = FPaths::GetExtension(FilePath).ToLower();

    // 如果是ZIP文件，需要解压
    if (FileExtension == TEXT("zip"))
    {
        UE_LOG(LogHunYuanDownload, Log, TEXT("FObjZipDownloadHandler: Detected ZIP file, extracting: %s"), *FilePath);

        FString ExtractedDir;
        if (FZipExtractor::Extract(FilePath, ExtractedDir))
        {
            FinalPath = ExtractedDir;
            UE_LOG(LogHunYuanDownload, Log, TEXT("FObjZipDownloadHandler: Extracted to: %s"), *ExtractedDir);

            // 查找解压后的模型文件
            FString ModelFile;
            if (FZipExtractor::FindModelFileInDirectory(ExtractedDir, ModelFile))
            {
                UE_LOG(LogHunYuanDownload, Log, TEXT("FObjZipDownloadHandler: Found model file: %s"), *ModelFile);
            }
        }
        else
        {
            UE_LOG(LogHunYuanDownload, Error, TEXT("FObjZipDownloadHandler: Extraction failed: %s"), *FilePath);
            return false;
        }
    }

    // 计算总大小
    int64 TotalSize = 0;
    if (PlatformFile.DirectoryExists(*FinalPath))
    {
        TArray<FString> AllFiles;
        PlatformFile.FindFilesRecursively(AllFiles, *FinalPath, TEXT("*"));
        for (const FString& File : AllFiles)
        {
            TotalSize += PlatformFile.FileSize(*File);
        }
    }
    else
    {
        TotalSize = PlatformFile.FileSize(*FinalPath);
    }

    OutModelInfo.FilePath = FinalPath;
    OutModelInfo.FileName = FPaths::GetBaseFilename(FilePath);
    OutModelInfo.Format = TEXT("OBJ");
    OutModelInfo.FileSize = TotalSize;
    OutModelInfo.JobId = JobId;

    UE_LOG(LogHunYuanDownload, Log, TEXT("FObjZipDownloadHandler: Processed OBJ file, total size: %lld bytes"), TotalSize);

    return true;
}

FString FObjZipDownloadHandler::GetFinalPath(const FModelInfo& ModelInfo) const
{
    return ModelInfo.FilePath;
}