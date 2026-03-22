// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UE_RAG_SDK : ModuleRules
{
	public UE_RAG_SDK(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
                "Core",
                "CoreUObject",
                "Engine",
                "HTTP",
				"Json",
                "JsonUtilities",
                "AssetRegistry"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
                "Slate",
                "SlateCore",
                "InputCore",
                "DesktopPlatform",
				"ImageWrapper"
            }
			);

        if (Target.Type == TargetType.Editor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }
	}
}
