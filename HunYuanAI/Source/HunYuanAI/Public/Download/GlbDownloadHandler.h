// Download/GlbDownloadHandler.h
#pragma once

#include "CoreMinimal.h"
#include "Download/IDownloadHandler.h"

class HUNYUANAI_API FGlbDownloadHandler : public IDownloadHandler
{
public:
    virtual bool CanHandle(const FString& URL) const override;
    virtual bool ProcessDownloadedFile(const FString& FilePath, const FString& JobId, FModelInfo& OutModelInfo) override;
    virtual FString GetFinalPath(const FModelInfo& ModelInfo) const override;
    virtual FString GetFormatName() const override { return TEXT("GLB"); }  // 这里定义，cpp中不要再定义
};