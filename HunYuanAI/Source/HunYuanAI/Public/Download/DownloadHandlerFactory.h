// DownloadHandlerFactory.h
#pragma once

#include "CoreMinimal.h"
#include "Download/IDownloadHandler.h"
#include "Download/SingleFileDownloadHandler.h"
#include "Download/ObjZipDownloadHandler.h"

class HUNYUANAI_API FDownloadHandlerFactory
{
public:
    // 根据URL获取对应的处理器
    static TSharedPtr<IDownloadHandler> GetHandler(const FString& URL);

    // 根据格式名称获取处理器
    static TSharedPtr<IDownloadHandler> GetHandlerByFormat(const FString& Format);

    // 注册自定义处理器（支持扩展）
    static void RegisterHandler(const FString& Format, TSharedPtr<IDownloadHandler> Handler);
    static void RegisterHandlerByExtension(const FString& Extension, TSharedPtr<IDownloadHandler> Handler);

    // 初始化内置处理器
    static void Initialize();

private:
    static TMap<FString, TSharedPtr<IDownloadHandler>> RegisteredHandlers;
    static TMap<FString, TSharedPtr<IDownloadHandler>> ExtensionHandlers;
    static bool bInitialized;
};