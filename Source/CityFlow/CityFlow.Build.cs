// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CityFlow : ModuleRules
{
	public CityFlow(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"ProceduralMeshComponent",
			"UMG",
			"Slate",
			"SlateCore",
			"DeveloperSettings",
			"Niagara",
			"Landscape"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {  });
	}
}
