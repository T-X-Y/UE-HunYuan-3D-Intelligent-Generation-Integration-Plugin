#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"

/**
 * ZIP解压工具类 - 使用7-Zip
 */
class HUNYUANAI_API FZipExtractor
{
public:
    // 检查是否为ZIP文件
    static bool IsZipFile(const FString& FilePath);

    // 解压ZIP文件到指定目录
    static bool Extract(const FString& ZipFilePath, FString& OutExtractedDir);

    // 在目录中查找模型文件
    static bool FindModelFileInDirectory(const FString& Directory, FString& OutModelFilePath);

    // 解压并查找模型文件（一步到位）
    static bool ExtractAndFindModel(const FString& ZipFilePath, FString& OutModelFilePath);

    static bool FindFilesWithSystemAPI(const FString& Directory, TArray<FString>& OutFiles);

private:
    static FString GenerateExtractDestination(const FString& ZipFilePath);
    static bool ExtractWith7Zip(const FString& ZipFilePath, const FString& DestDir);
    static FString Find7ZipPath();
};