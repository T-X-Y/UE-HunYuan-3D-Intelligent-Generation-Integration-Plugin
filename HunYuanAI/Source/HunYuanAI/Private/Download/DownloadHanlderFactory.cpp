// DownloadHandlerFactory.cpp
#include "Download/DownloadHandlerFactory.h"
#include "Logging/HunYuanLogging.h"

// 静态成员初始化
TMap<FString, TSharedPtr<IDownloadHandler>> FDownloadHandlerFactory::RegisteredHandlers;
TMap<FString, TSharedPtr<IDownloadHandler>> FDownloadHandlerFactory::ExtensionHandlers;
bool FDownloadHandlerFactory::bInitialized = false;

void FDownloadHandlerFactory::Initialize()
{
    if (bInitialized) return;

    // 注册单文件格式处理器
    RegisterHandlerByExtension(TEXT(".glb"), MakeShareable(new FSingleFileDownloadHandler(TEXT("GLB"))));
    RegisterHandlerByExtension(TEXT(".fbx"), MakeShareable(new FSingleFileDownloadHandler(TEXT("FBX"))));
    RegisterHandlerByExtension(TEXT(".stl"), MakeShareable(new FSingleFileDownloadHandler(TEXT("STL"))));
    RegisterHandlerByExtension(TEXT(".usdz"), MakeShareable(new FSingleFileDownloadHandler(TEXT("USDZ"))));
    RegisterHandlerByExtension(TEXT(".obj"), MakeShareable(new FSingleFileDownloadHandler(TEXT("OBJ"))));  // 单个OBJ文件

    // 注册需要特殊处理的格式（需要解压的ZIP/OBJ包）
    RegisterHandler(TEXT("OBJ_ZIP"), MakeShareable(new FObjZipDownloadHandler()));
    RegisterHandler(TEXT("ZIP"), MakeShareable(new FObjZipDownloadHandler()));

    bInitialized = true;

    UE_LOG(LogHunYuanDownload, Log, TEXT("DownloadHandlerFactory initialized with %d handlers"),
        RegisteredHandlers.Num() + ExtensionHandlers.Num());
}

TSharedPtr<IDownloadHandler> FDownloadHandlerFactory::GetHandler(const FString& URL)
{
    Initialize();

    FString LowerURL = URL.ToLower();

    // 1. 先检查注册的处理器
    for (const auto& Pair : RegisteredHandlers)
    {
        if (Pair.Value.IsValid() && Pair.Value->CanHandle(URL))
        {
            UE_LOG(LogHunYuanDownload, Verbose, TEXT("Using registered handler: %s for URL"), *Pair.Key);
            return Pair.Value;
        }
    }

    // 2. 根据扩展名匹配
    for (const auto& Pair : ExtensionHandlers)
    {
        if (LowerURL.EndsWith(Pair.Key) || LowerURL.Contains(Pair.Key + TEXT("?")))
        {
            UE_LOG(LogHunYuanDownload, Log, TEXT("Using extension handler: %s for URL: %s"), *Pair.Key, *URL);
            return Pair.Value;
        }
    }

    // 3. 默认使用 OBJ/ZIP 处理器
    //    因为如果URL没有扩展名或者无法识别，最可能是OBJ/ZIP格式
    UE_LOG(LogHunYuanDownload, Log, TEXT("Using default OBJ/ZIP handler for URL: %s"), *URL);
    return MakeShareable(new FObjZipDownloadHandler());
}

TSharedPtr<IDownloadHandler> FDownloadHandlerFactory::GetHandlerByFormat(const FString& Format)
{
    Initialize();

    FString UpperFormat = Format.ToUpper();

    // 检查注册的处理器
    if (RegisteredHandlers.Contains(UpperFormat))
    {
        return RegisteredHandlers[UpperFormat];
    }

    // 根据格式返回对应的处理器
    if (UpperFormat == TEXT("GLB") || UpperFormat == TEXT("FBX") ||
        UpperFormat == TEXT("STL") || UpperFormat == TEXT("USDZ") ||
        UpperFormat == TEXT("OBJ"))  // 单个OBJ文件也使用单文件处理器
    {
        return MakeShareable(new FSingleFileDownloadHandler(UpperFormat));
    }

    // 默认返回 OBJ/ZIP 处理器
    return MakeShareable(new FObjZipDownloadHandler());
}

void FDownloadHandlerFactory::RegisterHandler(const FString& Format, TSharedPtr<IDownloadHandler> Handler)
{
    if (Handler.IsValid())
    {
        RegisteredHandlers.Add(Format.ToUpper(), Handler);
        UE_LOG(LogHunYuanDownload, Log, TEXT("Registered handler for format: %s"), *Format);
    }
}

void FDownloadHandlerFactory::RegisterHandlerByExtension(const FString& Extension, TSharedPtr<IDownloadHandler> Handler)
{
    if (Handler.IsValid())
    {
        ExtensionHandlers.Add(Extension.ToLower(), Handler);
        UE_LOG(LogHunYuanDownload, Log, TEXT("Registered handler for extension: %s"), *Extension);
    }
}