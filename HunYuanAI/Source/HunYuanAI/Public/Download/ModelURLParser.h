#pragma once

#include "CoreMinimal.h"
#include "API/HunYuanAPITypes.h"

class HUNYUANAI_API FModelURLParser
{
public:
    // 从URL获取文件扩展名
    static FString GetFileExtensionFromURL(const FString& URL);

    // 从URL解析模型格式
    static HunYuanAPI::EModelFormat ParseModelFormat(const FString& URL);

    // 生成文件名（JobId_时间戳.扩展名）
    static FString GenerateFileName(const FString& JobId, const FString& URL);

    // 从URL获取文件名（不包含路径）
    static FString GetFileNameFromURL(const FString& URL);

    // 检查URL是否有效
    static bool IsValidURL(const FString& URL);

    // 从URL中提取基础URL（去掉查询参数）
    static FString GetBaseURL(const FString& URL);

    // 是否是ZIP文件URL
    static bool IsZipURL(const FString& URL);

private:
    static FString SanitizeFileName(const FString& FileName);
};