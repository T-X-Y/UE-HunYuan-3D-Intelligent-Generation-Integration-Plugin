// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "Config/HunYuanConfig.h"

#ifdef HUNYUANAI_HunYuanConfig_generated_h
#error "HunYuanConfig.generated.h already included, missing '#pragma once' in HunYuanConfig.h"
#endif
#define HUNYUANAI_HunYuanConfig_generated_h

#include "Templates/IsUEnumClass.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "Templates/NoDestroy.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_unreal_projects_HunYuanTest_Plugins_HunYuanAI_Source_HunYuanAI_Public_Config_HunYuanConfig_h

// ********** Begin Enum EConfigFormatPreference ***************************************************
#define FOREACH_ENUM_ECONFIGFORMATPREFERENCE(op) \
	op(EConfigFormatPreference::Default) \
	op(EConfigFormatPreference::GLB) \
	op(EConfigFormatPreference::OBJ) \
	op(EConfigFormatPreference::STL) \
	op(EConfigFormatPreference::USDZ) \
	op(EConfigFormatPreference::FBX) 

enum class EConfigFormatPreference : uint8;
template<> struct TIsUEnumClass<EConfigFormatPreference> { enum { Value = true }; };
template<> HUNYUANAI_NON_ATTRIBUTED_API UEnum* StaticEnum<EConfigFormatPreference>();
// ********** End Enum EConfigFormatPreference *****************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
