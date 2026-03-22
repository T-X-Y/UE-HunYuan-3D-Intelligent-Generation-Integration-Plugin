#pragma once

#include "CoreMinimal.h"
#include "UI/ModelInfoWidget.h"

/**
 * 下载处理器抽象接口
 */
class IDownloadHandler : public TSharedFromThis<IDownloadHandler>
{
public:
    virtual ~IDownloadHandler() {}

    // 判断是否支持该URL
    virtual bool CanHandle(const FString& URL) const = 0;

    // 获取格式名称
    virtual FString GetFormatName() const = 0;

    // 处理下载完成的文件
    virtual bool ProcessDownloadedFile(const FString& FilePath,const FString& JobId,FModelInfo& OutModelInfo) = 0;

    // 获取最终路径
    virtual FString GetFinalPath(const FModelInfo& ModelInfo) const = 0;

    // 是否需要解压
    virtual bool RequiresExtraction() const { return false; }

    // 是否可直接导入
    virtual bool CanImportDirectly() const { return true; }
};