#pragma once

#include "CoreMinimal.h"

class HUNYUANAI_API FZipExtractor
{
public:
    // 从ZIP文件中提取模型
    static bool ExtractModelFromZip(const FString& ZipFilePath, FString& OutModelPath);

    // 检查文件是否为ZIP文件
    static bool IsZipFile(const FString& FilePath);

    // 获取解压后的目录路径
    static FString GetExtractDestination(const FString& ZipFilePath);

private:
    static bool UnzipToDirectory(const FString& ZipFilePath, const FString& DestDir);
    static bool FindModelFileInDirectory(const FString& Directory, FString& OutModelPath);
    static bool ExecutePowerShellUnzip(const FString& ZipFilePath, const FString& DestDir);
};