// UAVMqttBridge 与 dock 后端 MQTT 的上云 API 协议桥接模块
using UnrealBuildTool;

public class UAVMqttBridge : ModuleRules
{
	public UAVMqttBridge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "UAVCore", "UAVDroneSim", "UAVFlightControl", "UAVCameraStream", "MQTTCore" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Json" });
	}
}
