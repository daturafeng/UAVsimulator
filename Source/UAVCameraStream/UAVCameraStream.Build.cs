// UAVCameraStream 相机载荷模拟与 RTMP 推流模块
using UnrealBuildTool;

public class UAVCameraStream : ModuleRules
{
	public UAVCameraStream(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "UAVCore", "UAVDroneSim" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Json", "RenderCore", "RHI" });
	}
}
