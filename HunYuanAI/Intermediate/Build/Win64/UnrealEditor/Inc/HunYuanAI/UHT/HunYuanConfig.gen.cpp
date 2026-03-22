// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "Config/HunYuanConfig.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodeHunYuanConfig() {}

// ********** Begin Cross Module References ********************************************************
HUNYUANAI_API UEnum* Z_Construct_UEnum_HunYuanAI_EConfigFormatPreference();
UPackage* Z_Construct_UPackage__Script_HunYuanAI();
// ********** End Cross Module References **********************************************************

// ********** Begin Enum EConfigFormatPreference ***************************************************
static FEnumRegistrationInfo Z_Registration_Info_UEnum_EConfigFormatPreference;
static UEnum* EConfigFormatPreference_StaticEnum()
{
	if (!Z_Registration_Info_UEnum_EConfigFormatPreference.OuterSingleton)
	{
		Z_Registration_Info_UEnum_EConfigFormatPreference.OuterSingleton = GetStaticEnum(Z_Construct_UEnum_HunYuanAI_EConfigFormatPreference, (UObject*)Z_Construct_UPackage__Script_HunYuanAI(), TEXT("EConfigFormatPreference"));
	}
	return Z_Registration_Info_UEnum_EConfigFormatPreference.OuterSingleton;
}
template<> HUNYUANAI_NON_ATTRIBUTED_API UEnum* StaticEnum<EConfigFormatPreference>()
{
	return EConfigFormatPreference_StaticEnum();
}
struct Z_Construct_UEnum_HunYuanAI_EConfigFormatPreference_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Enum_MetaDataParams[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "// \xe6\xa0\xbc\xe5\xbc\x8f\xe5\x81\x8f\xe5\xa5\xbd\xe6\x9e\x9a\xe4\xb8\xbe\xef\xbc\x88\xe4\xb8\x8e\xe4\xb9\x8b\xe5\x89\x8d\xe5\xae\x9a\xe4\xb9\x89\xe7\x9a\x84\xe4\xbf\x9d\xe6\x8c\x81\xe4\xb8\x80\xe8\x87\xb4\xef\xbc\x89\n" },
#endif
		{ "Default.DisplayName", "\xe9\xbb\x98\xe8\xae\xa4 (OBJ+GLB)" },
		{ "Default.Name", "EConfigFormatPreference::Default" },
		{ "FBX.DisplayName", "FBX\xe6\xa0\xbc\xe5\xbc\x8f (\xe5\x8a\xa8\xe7\x94\xbb)" },
		{ "FBX.Name", "EConfigFormatPreference::FBX" },
		{ "GLB.DisplayName", "GLB\xe6\xa0\xbc\xe5\xbc\x8f" },
		{ "GLB.Name", "EConfigFormatPreference::GLB" },
		{ "ModuleRelativePath", "Public/Config/HunYuanConfig.h" },
		{ "OBJ.DisplayName", "OBJ\xe6\xa0\xbc\xe5\xbc\x8f (ZIP)" },
		{ "OBJ.Name", "EConfigFormatPreference::OBJ" },
		{ "STL.DisplayName", "STL\xe6\xa0\xbc\xe5\xbc\x8f (3D\xe6\x89\x93\xe5\x8d\xb0)" },
		{ "STL.Name", "EConfigFormatPreference::STL" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe6\xa0\xbc\xe5\xbc\x8f\xe5\x81\x8f\xe5\xa5\xbd\xe6\x9e\x9a\xe4\xb8\xbe\xef\xbc\x88\xe4\xb8\x8e\xe4\xb9\x8b\xe5\x89\x8d\xe5\xae\x9a\xe4\xb9\x89\xe7\x9a\x84\xe4\xbf\x9d\xe6\x8c\x81\xe4\xb8\x80\xe8\x87\xb4\xef\xbc\x89" },
#endif
		{ "USDZ.DisplayName", "USDZ\xe6\xa0\xbc\xe5\xbc\x8f (AR)" },
		{ "USDZ.Name", "EConfigFormatPreference::USDZ" },
	};
#endif // WITH_METADATA
	static constexpr UECodeGen_Private::FEnumeratorParam Enumerators[] = {
		{ "EConfigFormatPreference::Default", (int64)EConfigFormatPreference::Default },
		{ "EConfigFormatPreference::GLB", (int64)EConfigFormatPreference::GLB },
		{ "EConfigFormatPreference::OBJ", (int64)EConfigFormatPreference::OBJ },
		{ "EConfigFormatPreference::STL", (int64)EConfigFormatPreference::STL },
		{ "EConfigFormatPreference::USDZ", (int64)EConfigFormatPreference::USDZ },
		{ "EConfigFormatPreference::FBX", (int64)EConfigFormatPreference::FBX },
	};
	static const UECodeGen_Private::FEnumParams EnumParams;
}; // struct Z_Construct_UEnum_HunYuanAI_EConfigFormatPreference_Statics 
const UECodeGen_Private::FEnumParams Z_Construct_UEnum_HunYuanAI_EConfigFormatPreference_Statics::EnumParams = {
	(UObject*(*)())Z_Construct_UPackage__Script_HunYuanAI,
	nullptr,
	"EConfigFormatPreference",
	"EConfigFormatPreference",
	Z_Construct_UEnum_HunYuanAI_EConfigFormatPreference_Statics::Enumerators,
	RF_Public|RF_Transient|RF_MarkAsNative,
	UE_ARRAY_COUNT(Z_Construct_UEnum_HunYuanAI_EConfigFormatPreference_Statics::Enumerators),
	EEnumFlags::None,
	(uint8)UEnum::ECppForm::EnumClass,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UEnum_HunYuanAI_EConfigFormatPreference_Statics::Enum_MetaDataParams), Z_Construct_UEnum_HunYuanAI_EConfigFormatPreference_Statics::Enum_MetaDataParams)
};
UEnum* Z_Construct_UEnum_HunYuanAI_EConfigFormatPreference()
{
	if (!Z_Registration_Info_UEnum_EConfigFormatPreference.InnerSingleton)
	{
		UECodeGen_Private::ConstructUEnum(Z_Registration_Info_UEnum_EConfigFormatPreference.InnerSingleton, Z_Construct_UEnum_HunYuanAI_EConfigFormatPreference_Statics::EnumParams);
	}
	return Z_Registration_Info_UEnum_EConfigFormatPreference.InnerSingleton;
}
// ********** End Enum EConfigFormatPreference *****************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_unreal_projects_HunYuanTest_Plugins_HunYuanAI_Source_HunYuanAI_Public_Config_HunYuanConfig_h__Script_HunYuanAI_Statics
{
	static constexpr FEnumRegisterCompiledInInfo EnumInfo[] = {
		{ EConfigFormatPreference_StaticEnum, TEXT("EConfigFormatPreference"), &Z_Registration_Info_UEnum_EConfigFormatPreference, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, 795362240U) },
	};
}; // Z_CompiledInDeferFile_FID_unreal_projects_HunYuanTest_Plugins_HunYuanAI_Source_HunYuanAI_Public_Config_HunYuanConfig_h__Script_HunYuanAI_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_unreal_projects_HunYuanTest_Plugins_HunYuanAI_Source_HunYuanAI_Public_Config_HunYuanConfig_h__Script_HunYuanAI_670711319{
	TEXT("/Script/HunYuanAI"),
	nullptr, 0,
	nullptr, 0,
	Z_CompiledInDeferFile_FID_unreal_projects_HunYuanTest_Plugins_HunYuanAI_Source_HunYuanAI_Public_Config_HunYuanConfig_h__Script_HunYuanAI_Statics::EnumInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_unreal_projects_HunYuanTest_Plugins_HunYuanAI_Source_HunYuanAI_Public_Config_HunYuanConfig_h__Script_HunYuanAI_Statics::EnumInfo),
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
