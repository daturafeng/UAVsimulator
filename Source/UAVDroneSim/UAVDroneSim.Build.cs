// UAVDroneSim 无人机本体模拟模块
using UnrealBuildTool;

public class UAVDroneSim : ModuleRules
{
	public UAVDroneSim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "UAVCore" });
		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
