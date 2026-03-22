// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "UI/SAIChatWindow.h"

#ifdef HUNYUANAI_SAIChatWindow_generated_h
#error "SAIChatWindow.generated.h already included, missing '#pragma once' in SAIChatWindow.h"
#endif
#define HUNYUANAI_SAIChatWindow_generated_h

#include "Templates/IsUEnumClass.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "Templates/NoDestroy.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_unreal_projects_HunYuanTest_Plugins_HunYuanAI_Source_HunYuanAI_Public_UI_SAIChatWindow_h

// ********** Begin Enum EModelFormatPreference ****************************************************
#define FOREACH_ENUM_EMODELFORMATPREFERENCE(op) \
	op(EModelFormatPreference::Default) \
	op(EModelFormatPreference::GLB) \
	op(EModelFormatPreference::OBJ) \
	op(EModelFormatPreference::STL) \
	op(EModelFormatPreference::USDZ) \
	op(EModelFormatPreference::FBX) 

enum class EModelFormatPreference : uint8;
template<> struct TIsUEnumClass<EModelFormatPreference> { enum { Value = true }; };
template<> HUNYUANAI_NON_ATTRIBUTED_API UEnum* StaticEnum<EModelFormatPreference>();
// ********** End Enum EModelFormatPreference ******************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
