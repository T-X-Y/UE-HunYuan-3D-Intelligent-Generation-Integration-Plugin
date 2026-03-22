// SingleFileDownloadHandler.h
#pragma once

#include "CoreMinimal.h"
#include "Download/IDownloadHandler.h"

class HUNYUANAI_API FSingleFileDownloadHandler : public IDownloadHandler
{
public:
    FSingleFileDownloadHandler(const FString& InFormat);

    virtual bool CanHandle(const FString& URL) const override;
    virtual bool ProcessDownloadedFile(const FString& FilePath, const FString& JobId, FModelInfo& OutModelInfo) override;
    virtual FString GetFinalPath(const FModelInfo& ModelInfo) const override;
    virtual FString GetFormatName() const override { return Format; }

private:
    FString Format;
    FString Extension;
};