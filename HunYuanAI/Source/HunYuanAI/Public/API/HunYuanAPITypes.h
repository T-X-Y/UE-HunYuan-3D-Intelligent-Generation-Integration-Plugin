#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// API 请求完成委托
DECLARE_DELEGATE_OneParam(FOnAPIRequestComplete, const TSharedPtr<FJsonObject>& /* Result */);

namespace HunYuanAPI
{
    // API 响应状态
    enum class EJobStatus
    {
        Unknown,
        Waiting,    // WAIT
        Running,    // RUN
        Completed,  // DONE
        Failed,     // FAIL
        Cancelled   // CANCEL
    };

    // 模型格式
    enum class EModelFormat
    {
        Unknown,
        GLB,
        GLTF,
        FBX,
        OBJ,
        STL,
        ThreeDS
    };

    // 任务类型
    enum class EJobType
    {
        TextTo3D,
        ImageTo3D
    };

    // 将字符串转换为EJobStatus
    inline EJobStatus StringToJobStatus(const FString& Status)
    {
        if (Status == TEXT("WAIT")) return EJobStatus::Waiting;
        if (Status == TEXT("RUN")) return EJobStatus::Running;
        if (Status == TEXT("DONE")) return EJobStatus::Completed;
        if (Status == TEXT("FAIL")) return EJobStatus::Failed;
        if (Status == TEXT("CANCEL")) return EJobStatus::Cancelled;
        return EJobStatus::Unknown;
    }

    // 将字符串转换为EModelFormat
    inline EModelFormat StringToModelFormat(const FString& Format)
    {
        FString LowerFormat = Format.ToLower();
        if (LowerFormat == TEXT("glb")) return EModelFormat::GLB;
        if (LowerFormat == TEXT("gltf")) return EModelFormat::GLTF;
        if (LowerFormat == TEXT("fbx")) return EModelFormat::FBX;
        if (LowerFormat == TEXT("obj")) return EModelFormat::OBJ;
        if (LowerFormat == TEXT("stl")) return EModelFormat::STL;
        if (LowerFormat == TEXT("3ds")) return EModelFormat::ThreeDS;
        return EModelFormat::Unknown;
    }

    // 获取模型格式对应的扩展名
    inline FString GetModelFormatExtension(EModelFormat Format)
    {
        switch (Format)
        {
        case EModelFormat::GLB: return TEXT(".glb");
        case EModelFormat::GLTF: return TEXT(".gltf");
        case EModelFormat::FBX: return TEXT(".fbx");
        case EModelFormat::OBJ: return TEXT(".obj");
        case EModelFormat::STL: return TEXT(".stl");
        case EModelFormat::ThreeDS: return TEXT(".3ds");
        default: return TEXT(".zip");
        }
    }
}

// 任务提交回调
DECLARE_DELEGATE_TwoParams(FOnJobSubmitted, bool /* bSuccess */, const FString& /* JobIdOrError */);
// 任务查询回调
DECLARE_DELEGATE_TwoParams(FOnJobQueried, bool /* bSuccess */, const TSharedPtr<FJsonObject>& /* Result */);