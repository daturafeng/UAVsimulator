// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UAVMQTTCore : ModuleRules
{
	public UAVMQTTCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"CoreUObject",
				"JsonBlueprintUtilities",
				"JsonUtilities",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"Engine",
				"Json",
				"JsonBlueprintUtilities",
				"Networking",
				"Projects",
				"Sockets"
			}
		);
		
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new[] {
					"FunctionalTesting",
					"UnrealEd"
				});
		}
	}
}
