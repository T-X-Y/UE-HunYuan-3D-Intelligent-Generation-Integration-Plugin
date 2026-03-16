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
        FString Format;
        int64 Size = 0;

        bool ParseFromJson(const TSharedPtr<FJsonObject>& JsonObject)
        {
            if (!JsonObject.IsValid()) return false;

            JsonObject->TryGetStringField(TEXT("Url"), Url);
            JsonObject->TryGetStringField(TEXT("Format"), Format);
            JsonObject->TryGetNumberField(TEXT("Size"), Size);

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

        bool ParseFromJson(const TSharedPtr<FJsonObject>& JsonObject)
        {
            if (!JsonObject.IsValid()) return false;

            JsonObject->TryGetStringField(TEXT("JobId"), JobId);
            JsonObject->TryGetStringField(TEXT("RequestId"), RequestId);

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
                for (const auto& Item : *FileArray)
                {
                    TSharedPtr<FJsonObject> FileObj = Item->AsObject();
                    if (FileObj.IsValid())
                    {
                        FModelFileInfo FileInfo;
                        if (FileInfo.ParseFromJson(FileObj))
                        {
                            ModelFiles.Add(FileInfo);
                        }
                    }
                }
            }

            return true;
        }

        bool IsCompleted() const { return Status == EJobStatus::Completed; }
        bool IsFailed() const { return Status == EJobStatus::Failed; }
        bool IsProcessing() const { return Status == EJobStatus::Waiting || Status == EJobStatus::Running; }

        FString GetFirstModelUrl() const
        {
            return ModelFiles.Num() > 0 ? ModelFiles[0].Url : FString();
        }
    };
}