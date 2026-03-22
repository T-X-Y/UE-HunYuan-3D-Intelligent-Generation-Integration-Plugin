// ObjZipDownloadHandler.h
#pragma once

#include "CoreMinimal.h"
#include "Download/IDownloadHandler.h"
#include "Download/ZipExtractor.h"

class HUNYUANAI_API FObjZipDownloadHandler : public IDownloadHandler
{
public:
    virtual bool CanHandle(const FString& URL) const override;
    virtual bool ProcessDownloadedFile(const FString& FilePath, const FString& JobId, FModelInfo& OutModelInfo) override;
    virtual FString GetFinalPath(const FModelInfo& ModelInfo) const override;
    virtual FString GetFormatName() const override { return TEXT("OBJ/ZIP"); }
    virtual bool RequiresExtraction() const override { return true; }
};