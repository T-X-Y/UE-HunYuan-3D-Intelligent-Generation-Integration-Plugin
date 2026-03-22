// SingleFileDownloadHandler.cpp
#include "Download/SingleFileDownloadHandler.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Logging/HunYuanLogging.h"

FSingleFileDownloadHandler::FSingleFileDownloadHandler(const FString& InFormat)
    : Format(InFormat)
{
    Extension = TEXT(".") + Format.ToLower();
}

bool FSingleFileDownloadHandler::CanHandle(const FString& URL) const
{
    FString LowerURL = URL.ToLower();
    return LowerURL.EndsWith(Extension) || LowerURL.Contains(Extension + TEXT("?"));
}

bool FSingleFileDownloadHandler::ProcessDownloadedFile(
    const FString& FilePath,
    const FString& JobId,
    FModelInfo& OutModelInfo)
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    if (!PlatformFile.FileExists(*FilePath))
    {
        UE_LOG(LogHunYuanDownload, Error, TEXT("FSingleFileDownloadHandler: File not found - %s"), *FilePath);
        return false;
    }

    OutModelInfo.FilePath = FilePath;
    OutModelInfo.FileName = FPaths::GetCleanFilename(FilePath);
    OutModelInfo.Format = Format;
    OutModelInfo.FileSize = PlatformFile.FileSize(*FilePath);
    OutModelInfo.JobId = JobId;

    UE_LOG(LogHunYuanDownload, Log, TEXT("FSingleFileDownloadHandler: Processed %s file: %s (%lld bytes)"),
        *Format, *OutModelInfo.FileName, OutModelInfo.FileSize);

    return true;
}

FString FSingleFileDownloadHandler::GetFinalPath(const FModelInfo& ModelInfo) const
{
    return ModelInfo.FilePath;
}