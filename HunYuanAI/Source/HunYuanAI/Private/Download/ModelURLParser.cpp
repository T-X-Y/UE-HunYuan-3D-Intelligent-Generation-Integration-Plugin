#include "Download/ModelURLParser.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"

FString FModelURLParser::GetFileExtensionFromURL(const FString& URL)
{
    FString BaseURL = GetBaseURL(URL);
    FString Extension = FPaths::GetExtension(BaseURL).ToLower();

    // 如果没有扩展名，尝试从内容类型判断
    if (Extension.IsEmpty())
    {
        // 默认返回.zip（腾讯云返回的是ZIP）
        return TEXT(".zip");
    }

    return TEXT(".") + Extension;
}

HunYuanAPI::EModelFormat FModelURLParser::ParseModelFormat(const FString& URL)
{
    FString Extension = GetFileExtensionFromURL(URL);

    if (Extension == TEXT(".glb")) return HunYuanAPI::EModelFormat::GLB;
    if (Extension == TEXT(".gltf")) return HunYuanAPI::EModelFormat::GLTF;
    if (Extension == TEXT(".fbx")) return HunYuanAPI::EModelFormat::FBX;
    if (Extension == TEXT(".obj")) return HunYuanAPI::EModelFormat::OBJ;
    if (Extension == TEXT(".stl")) return HunYuanAPI::EModelFormat::STL;
    if (Extension == TEXT(".3ds")) return HunYuanAPI::EModelFormat::ThreeDS;

    return HunYuanAPI::EModelFormat::Unknown;
}

FString FModelURLParser::GenerateFileName(const FString& JobId, const FString& URL)
{
    FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
    FString Extension = GetFileExtensionFromURL(URL);

    // 清理JobId中的非法字符
    FString SafeJobId = SanitizeFileName(JobId);

    return FString::Printf(TEXT("%s_%s%s"), *SafeJobId, *Timestamp, *Extension);
}

FString FModelURLParser::GetFileNameFromURL(const FString& URL)
{
    FString BaseURL = GetBaseURL(URL);
    return FPaths::GetCleanFilename(BaseURL);
}

bool FModelURLParser::IsValidURL(const FString& URL)
{
    return URL.StartsWith(TEXT("http://")) || URL.StartsWith(TEXT("https://"));
}

FString FModelURLParser::GetBaseURL(const FString& URL)
{
    int32 QueryPos = URL.Find(TEXT("?"));
    if (QueryPos != INDEX_NONE)
    {
        return URL.Left(QueryPos);
    }
    return URL;
}

bool FModelURLParser::IsZipURL(const FString& URL)
{
    FString BaseURL = GetBaseURL(URL).ToLower();
    return BaseURL.EndsWith(TEXT(".zip")) || BaseURL.Contains(TEXT(".zip?"));
}

FString FModelURLParser::SanitizeFileName(const FString& FileName)
{
    FString Result = FileName;

    // 替换Windows文件名中不允许的字符
    Result = Result.Replace(TEXT("\\"), TEXT("_"))
        .Replace(TEXT("/"), TEXT("_"))
        .Replace(TEXT(":"), TEXT("_"))
        .Replace(TEXT("*"), TEXT("_"))
        .Replace(TEXT("?"), TEXT("_"))
        .Replace(TEXT("\""), TEXT("_"))
        .Replace(TEXT("<"), TEXT("_"))
        .Replace(TEXT(">"), TEXT("_"))
        .Replace(TEXT("|"), TEXT("_"));

    return Result;
}