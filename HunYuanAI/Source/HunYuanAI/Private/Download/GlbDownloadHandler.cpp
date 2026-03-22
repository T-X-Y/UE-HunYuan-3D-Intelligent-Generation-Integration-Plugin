// GlbDownloadHandler.cpp
#include "Download/GlbDownloadHandler.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "UI/ModelInfoWidget.h"
#include "Logging/HunYuanLogging.h"

bool FGlbDownloadHandler::CanHandle(const FString& URL) const
{
    FString LowerURL = URL.ToLower();
    return LowerURL.EndsWith(TEXT(".glb")) ||
        LowerURL.Contains(TEXT(".glb?"));
}

bool FGlbDownloadHandler::ProcessDownloadedFile(
    const FString& FilePath,
    const FString& JobId,
    FModelInfo& OutModelInfo)
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    if (!PlatformFile.FileExists(*FilePath))
    {
        UE_LOG(LogHunYuanDownload, Error, TEXT("FGlbDownloadHandler: File not found - %s"), *FilePath);
        return false;
    }

    OutModelInfo.FilePath = FilePath;
    OutModelInfo.FileName = FPaths::GetCleanFilename(FilePath);
    OutModelInfo.Format = TEXT("GLB");
    OutModelInfo.FileSize = PlatformFile.FileSize(*FilePath);
    OutModelInfo.JobId = JobId;

    UE_LOG(LogHunYuanDownload, Log, TEXT("FGlbDownloadHandler: 处理GLB文件: %s (%lld 字节)"),
        *OutModelInfo.FileName, OutModelInfo.FileSize);

    return true;
}

FString FGlbDownloadHandler::GetFinalPath(const FModelInfo& ModelInfo) const
{
    return ModelInfo.FilePath;
}