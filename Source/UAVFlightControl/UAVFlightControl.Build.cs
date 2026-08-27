// UAVFlightControl 飞控指令模块
using UnrealBuildTool;

public class UAVFlightControl : ModuleRules
{
	public UAVFlightControl(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "UAVCore", "UAVDroneSim" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Json" });
	}
}
