// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UAVMQTTCoreEditor : ModuleRules
{
	public UAVMQTTCoreEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new[] {
				"Core",
                "CoreUObject"
            });

		PrivateDependencyModuleNames.AddRange(
			new[] {
				"Engine",
				"UAVMQTTCore",
				"Settings",
				"Slate",
				"SlateCore",
				"UnrealEd"
			});
    }
}
