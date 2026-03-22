// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "UI/SAIChatWindow.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodeSAIChatWindow() {}

// ********** Begin Cross Module References ********************************************************
HUNYUANAI_API UEnum* Z_Construct_UEnum_HunYuanAI_EModelFormatPreference();
UPackage* Z_Construct_UPackage__Script_HunYuanAI();
// ********** End Cross Module References **********************************************************

// ********** Begin Enum EModelFormatPreference ****************************************************
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EModelFormatPreference;
static UEnum* EModelFormatPreference_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EModelFormatPreference.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EModelFormatPreference.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_HunYuanAI_EModelFormatPreference, (UObject*)Z_Construct_UPackage__Script_HunYuanAI(), TEXT("EModelFormatPreference"));
	}
	return Z_Registration_Info_UEnum_EModelFormatPreference.OuterSingleton;
}
template<> HUNYUANAI_NON_ATTRIBUTED_API UEnum* StaticEnum<EModelFormatPreference>()
{
	return EModelFormatPreference_StaticEnum();
}
struct Z_Construct_UEnum_HunYuanAI_EModelFormatPreference_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// \xe6\xa0\xbc\xe5\xbc\x8f\xe5\x81\x8f\xe5\xa5\xbd\xe6\x9e\x9a\xe4\xb8\xbe\n" },
#endif
		{ "Default.DisplayName", "\xe9\xbb\x98\xe8\xae\xa4 (OBJ+GLB)" },
		{ "Default.Name", "EModelFormatPreference::Default" },
		{ "FBX.DisplayName", "FBX\xe6\xa0\xbc\xe5\xbc\x8f (\xe5\x8a\xa8\xe7\x94\xbb)" },
		{ "FBX.Name", "EModelFormatPreference::FBX" },
		{ "GLB.DisplayName", "GLB\xe6\xa0\xbc\xe5\xbc\x8f" },
		{ "GLB.Name", "EModelFormatPreference::GLB" },
		{ "ModuleRelativePath", "Public/UI/SAIChatWindow.h" },
		{ "OBJ.DisplayName", "OBJ\xe6\xa0\xbc\xe5\xbc\x8f (ZIP)" },
		{ "OBJ.Name", "EModelFormatPreference::OBJ" },
		{ "STL.DisplayName", "STL\xe6\xa0\xbc\xe5\xbc\x8f (3D\xe6\x89\x93\xe5\x8d\xb0)" },
		{ "STL.Name", "EModelFormatPreference::STL" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe6\xa0\xbc\xe5\xbc\x8f\xe5\x81\x8f\xe5\xa5\xbd\xe6\x9e\x9a\xe4\xb8\xbe" },
#endif
		{ "USDZ.DisplayName", "USDZ\xe6\xa0\xbc\xe5\xbc\x8f (AR)" },
		{ "USDZ.Name", "EModelFormatPreference::USDZ" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EModelFormatPreference::Default", (int64)EModelFormatPreference::Default },
		{ "EModelFormatPreference::GLB", (int64)EModelFormatPreference::GLB },
		{ "EModelFormatPreference::OBJ", (int64)EModelFormatPreference::OBJ },
		{ "EModelFormatPreference::STL", (int64)EModelFormatPreference::STL },
		{ "EModelFormatPreference::USDZ", (int64)EModelFormatPreference::USDZ },
		{ "EModelFormatPreference::FBX", (int64)EModelFormatPreference::FBX },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct Z_Construct_UEnum_HunYuanAI_EModelFormatPreference_Statics 
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_HunYuanAI_EModelFormatPreference_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_HunYuanAI,
	nullptr,
	"EModelFormatPreference",
	"EModelFormatPreference",
	Z_Construct_UEnum_HunYuanAI_EModelFormatPreference_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_HunYuanAI_EModelFormatPreference_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_HunYuanAI_EModelFormatPreference_Statics::Enum_MetaDataParams), Z_Construct_UEnum_HunYuanAI_EModelFormatPreference_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_HunYuanAI_EModelFormatPreference()
{
	if (!Z_Registration_Info_UEnum_EModelFormatPreference.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EModelFormatPreference.InnerSingleton, Z_Construct_UEnum_HunYuanAI_EModelFormatPreference_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EModelFormatPreference.InnerSingleton;
}
// ********** End Enum EModelFormatPreference ******************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_unreal_projects_HunYuanTest_Plugins_HunYuanAI_Source_HunYuanAI_Public_UI_SAIChatWindow_h__Script_HunYuanAI_Statics
{
	static constexpr FEnumRegisterCompiledInInfo EnumInfo[] = {
		{ EModelFormatPreference_StaticEnum, TEXT("EModelFormatPreference"), &Z_Registration_Info_UEnum_EModelFormatPreference, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 4236913971U) },
	};
}; // Z_CompiledInDeferFile_FID_unreal_projects_HunYuanTest_Plugins_HunYuanAI_Source_HunYuanAI_Public_UI_SAIChatWindow_h__Script_HunYuanAI_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_unreal_projects_HunYuanTest_Plugins_HunYuanAI_Source_HunYuanAI_Public_UI_SAIChatWindow_h__Script_HunYuanAI_3201388314{
	TEXT("/Script/HunYuanAI"),
	nullptr, 0,
	nullptr, 0,
	Z_CompiledInDeferFile_FID_unreal_projects_HunYuanTest_Plugins_HunYuanAI_Source_HunYuanAI_Public_UI_SAIChatWindow_h__Script_HunYuanAI_Statics::EnumInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_unreal_projects_HunYuanTest_Plugins_HunYuanAI_Source_HunYuanAI_Public_UI_SAIChatWindow_h__Script_HunYuanAI_Statics::EnumInfo),
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
