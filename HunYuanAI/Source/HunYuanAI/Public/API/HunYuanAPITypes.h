#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Misc/Base64.h"

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

    // 模型格式枚举
    UENUM(BlueprintType)
        enum class EModelFormat : uint8
    {
        Default     UMETA(DisplayName = "默认 (OBJ+GLB)"),
        GLB         UMETA(DisplayName = "GLB格式"),
        OBJ         UMETA(DisplayName = "OBJ格式 (ZIP)"),
        STL         UMETA(DisplayName = "STL格式 (3D打印)"),
        USDZ        UMETA(DisplayName = "USDZ格式 (AR)"),
        FBX         UMETA(DisplayName = "FBX格式 (动画)")
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
        if (LowerFormat == TEXT("obj")) return EModelFormat::OBJ;
        if (LowerFormat == TEXT("stl")) return EModelFormat::STL;
        if (LowerFormat == TEXT("usdz")) return EModelFormat::USDZ;
        if (LowerFormat == TEXT("fbx")) return EModelFormat::FBX;
        return EModelFormat::Default;
    }

    // 获取模型格式对应的扩展名
    inline FString GetModelFormatExtension(EModelFormat Format)
    {
        switch (Format)
        {
        case EModelFormat::GLB: return TEXT(".glb");
        case EModelFormat::OBJ: return TEXT(".obj");
        case EModelFormat::STL: return TEXT(".stl");
        case EModelFormat::USDZ: return TEXT(".usdz");
        case EModelFormat::FBX: return TEXT(".fbx");
        default: return TEXT(".zip");
        }
    }

    // 获取格式字符串（用于API请求）
    inline FString GetFormatString(EModelFormat Format)
    {
        switch (Format)
        {
        case EModelFormat::GLB: return TEXT("GLB");
        case EModelFormat::OBJ: return TEXT("OBJ");
        case EModelFormat::STL: return TEXT("STL");
        case EModelFormat::USDZ: return TEXT("USDZ");
        case EModelFormat::FBX: return TEXT("FBX");
        case EModelFormat::Default:
        default: return TEXT("");
        }
    }

    // 视图类型枚举
    UENUM(BlueprintType)
        enum class EViewType : uint8
    {
        Front       UMETA(DisplayName = "正视图"),
        Back        UMETA(DisplayName = "背视图"),
        Left        UMETA(DisplayName = "左视图"),
        Right       UMETA(DisplayName = "右视图"),
        Top         UMETA(DisplayName = "顶视图"),
        Bottom      UMETA(DisplayName = "底视图"),
        Left45      UMETA(DisplayName = "左45度"),
        Right45     UMETA(DisplayName = "右45度")
    };

    // 多视图图片信息
    struct FMultiViewImage
    {
        EViewType ViewType;
        FString FilePath;
        TArray<uint8> ImageData;
        FString Base64Data;
        bool bIsValid = false;

        FString GetViewTypeString() const
        {
            switch (ViewType)
            {
            case EViewType::Front:   return TEXT("front");
            case EViewType::Back:    return TEXT("back");
            case EViewType::Left:    return TEXT("left");
            case EViewType::Right:   return TEXT("right");
            case EViewType::Top:     return TEXT("top");
            case EViewType::Bottom:  return TEXT("bottom");
            case EViewType::Left45:  return TEXT("left_front");
            case EViewType::Right45: return TEXT("right_front");
            default: return TEXT("front");
            }
        }
    };

    // 多视图输入数据
    struct FMultiViewInput
    {
        TArray<FMultiViewImage> Images;

        // 添加图片
        void AddImage(EViewType ViewType, const FString& FilePath, const TArray<uint8>& ImageData)
        {
            FMultiViewImage Image;
            Image.ViewType = ViewType;
            Image.FilePath = FilePath;
            Image.ImageData = ImageData;
            Image.Base64Data = FBase64::Encode(ImageData);
            Image.bIsValid = true;
            Images.Add(Image);
        }

        // 获取有效图片数量
        int32 GetValidImageCount() const
        {
            int32 Count = 0;
            for (const auto& Image : Images)
            {
                if (Image.bIsValid) Count++;
            }
            return Count;
        }

        // 是否有足够的图片（至少1张，最多8张）
        bool IsValid() const
        {
            int32 Count = GetValidImageCount();
            return Count >= 1 && Count <= 8;
        }
    };
}

// 任务提交回调
DECLARE_DELEGATE_TwoParams(FOnJobSubmitted, bool /* bSuccess */, const FString& /* JobIdOrError */);
// 任务查询回调
DECLARE_DELEGATE_TwoParams(FOnJobQueried, bool /* bSuccess */, const TSharedPtr<FJsonObject>& /* Result */);
// API 请求完成委托
DECLARE_DELEGATE_OneParam(FOnAPIRequestComplete, const TSharedPtr<FJsonObject>& /* Result */);