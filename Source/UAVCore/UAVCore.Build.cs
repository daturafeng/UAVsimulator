// UAVCore 公共依赖基座模块
using UnrealBuildTool;

public class UAVCore : ModuleRules
{
	public UAVCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Json" });
	}
}
