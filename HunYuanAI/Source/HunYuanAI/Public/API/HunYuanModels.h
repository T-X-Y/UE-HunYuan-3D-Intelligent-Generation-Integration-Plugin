#pragma once

#include "CoreMinimal.h"
#include "HunYuanAPITypes.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace HunYuanAPI
{
    // 3D模型文件信息
    struct FModelFileInfo
    {
        FString Url;
        FString Format;  // 我们内部使用 Format，但API返回的是 Type
        int64 Size = 0;

        bool ParseFromJson(const TSharedPtr<FJsonObject>& JsonObject)
        {
            if (!JsonObject.IsValid()) return false;

            JsonObject->TryGetStringField(TEXT("Url"), Url);

            // 重要：API返回的是 "Type" 字段，不是 "Format"
            // 同时兼容两种字段名
            if (!JsonObject->TryGetStringField(TEXT("Type"), Format))
            {
                // 如果 Type 不存在，尝试 Format
                JsonObject->TryGetStringField(TEXT("Format"), Format);
            }

            JsonObject->TryGetNumberField(TEXT("Size"), Size);

            // 如果格式为空，尝试从URL中提取
            if (Format.IsEmpty() && !Url.IsEmpty())
            {
                FString LowerURL = Url.ToLower();
                if (LowerURL.EndsWith(TEXT(".zip")) || LowerURL.Contains(TEXT(".zip?")))
                    Format = TEXT("OBJ");
                else if (LowerURL.EndsWith(TEXT(".glb")) || LowerURL.Contains(TEXT(".glb?")))
                    Format = TEXT("GLB");
                else if (LowerURL.EndsWith(TEXT(".fbx")) || LowerURL.Contains(TEXT(".fbx?")))
                    Format = TEXT("FBX");
                else if (LowerURL.EndsWith(TEXT(".stl")) || LowerURL.Contains(TEXT(".stl?")))
                    Format = TEXT("STL");
                else if (LowerURL.EndsWith(TEXT(".usdz")) || LowerURL.Contains(TEXT(".usdz?")))
                    Format = TEXT("USDZ");
            }

            UE_LOG(LogTemp, Verbose, TEXT("FModelFileInfo::ParseFromJson - Format=%s, Url=%s"), *Format, *Url);

            return !Url.IsEmpty();
        }
    };

    // 任务结果
    struct FJobResult
    {
        FString JobId;
        EJobStatus Status = EJobStatus::Unknown;
        FString ErrorCode;
        FString ErrorMessage;
        TArray<FModelFileInfo> ModelFiles;
        FString RequestId;
        FString ResultCreditDetails;
        int32 ResultCreditConsumed = 0;

        bool ParseFromJson(const TSharedPtr<FJsonObject>& JsonObject)
        {
            if (!JsonObject.IsValid()) return false;

            JsonObject->TryGetStringField(TEXT("JobId"), JobId);
            JsonObject->TryGetStringField(TEXT("RequestId"), RequestId);
            JsonObject->TryGetStringField(TEXT("ResultCreditDetails"), ResultCreditDetails);
            JsonObject->TryGetNumberField(TEXT("ResultCreditConsumed"), ResultCreditConsumed);

            // 解析状态
            FString StatusStr;
            if (JsonObject->TryGetStringField(TEXT("Status"), StatusStr))
            {
                Status = StringToJobStatus(StatusStr);
            }

            // 解析错误信息
            JsonObject->TryGetStringField(TEXT("ErrorCode"), ErrorCode);
            JsonObject->TryGetStringField(TEXT("ErrorMessage"), ErrorMessage);

            // 解析模型文件
            const TArray<TSharedPtr<FJsonValue>>* FileArray = nullptr;
            if (JsonObject->TryGetArrayField(TEXT("ResultFile3Ds"), FileArray))
            {
                ModelFiles.Empty();
                for (const auto& Item : *FileArray)
                {
                    TSharedPtr<FJsonObject> FileObj = Item->AsObject();
                    if (FileObj.IsValid())
                    {
                        FModelFileInfo FileInfo;
                        if (FileInfo.ParseFromJson(FileObj))
                        {
                            ModelFiles.Add(FileInfo);
                            UE_LOG(LogTemp, Log, TEXT("解析到模型文件: Format=%s, Url=%s"),
                                *FileInfo.Format, *FileInfo.Url);
                        }
                    }
                }
            }

            UE_LOG(LogTemp, Log, TEXT("FJobResult::ParseFromJson - Status=%s, 文件数量=%d"),
                *StatusStr, ModelFiles.Num());

            return true;
        }

        bool IsCompleted() const { return Status == EJobStatus::Completed; }
        bool IsFailed() const { return Status == EJobStatus::Failed; }
        bool IsProcessing() const { return Status == EJobStatus::Waiting || Status == EJobStatus::Running; }

        FString GetFirstModelUrl() const
        {
            return ModelFiles.Num() > 0 ? ModelFiles[0].Url : FString();
        }

        // 根据格式获取模型URL
        FString GetModelUrlByFormat(const FString& TargetFormat) const
        {
            for (const auto& File : ModelFiles)
            {
                if (File.Format.Equals(TargetFormat, ESearchCase::IgnoreCase))
                {
                    return File.Url;
                }
            }
            return FString();
        }

        // 获取所有可用格式
        TArray<FString> GetAvailableFormats() const
        {
            TArray<FString> Formats;
            for (const auto& File : ModelFiles)
            {
                Formats.Add(File.Format);
            }
            return Formats;
        }
    };
}