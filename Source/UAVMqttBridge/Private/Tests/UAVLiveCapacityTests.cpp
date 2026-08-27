// 直播能力 state 自动化测试：报文结构完整性（任务 2.x）
#include "Misc/AutomationTest.h"
#include "UAVMqttBridgeComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVLiveCapacityStructureTest, "UAV.MqttBridge.LiveCapacity.Structure", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVLiveCapacityStructureTest::RunTest(const FString& Parameters)
{
	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> Data = Bridge->BuildLiveCapacityPayload();
	TestTrue(TEXT("直播能力 data 已组装"), Data.IsValid());
	if (!Data.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject> Capacity = Data->GetObjectField(TEXT("live_capacity"));
	TestTrue(TEXT("包含 live_capacity 对象"), Capacity.IsValid());
	if (!Capacity.IsValid())
	{
		return false;
	}

	TestEqual(TEXT("available_video_number=3"), 3.0, Capacity->GetNumberField(TEXT("available_video_number")));
	TestEqual(TEXT("coexist_video_number_max=3"), 3.0, Capacity->GetNumberField(TEXT("coexist_video_number_max")));

	const TArray<TSharedPtr<FJsonValue>> DeviceList = Capacity->GetArrayField(TEXT("device_list"));
	TestEqual(TEXT("device_list 含 2 个设备项"), 2, DeviceList.Num());
	if (DeviceList.Num() != 2)
	{
		return false;
	}

	// 网关设备项（机场）：165-0-7 相机
	const TSharedPtr<FJsonObject> Gateway = DeviceList[0]->AsObject();
	TestTrue(TEXT("网关设备项可解析"), Gateway.IsValid());
	if (Gateway.IsValid())
	{
		TestEqual(TEXT("网关设备 sn=机场SN"), Bridge->DockSn, Gateway->GetStringField(TEXT("sn")));
		TestEqual(TEXT("网关 available_video_number=1"), 1.0, Gateway->GetNumberField(TEXT("available_video_number")));
		const TArray<TSharedPtr<FJsonValue>> GatewayCameras = Gateway->GetArrayField(TEXT("camera_list"));
		TestEqual(TEXT("网关 camera_list 含 1 项"), 1, GatewayCameras.Num());
		if (GatewayCameras.Num() > 0)
		{
			const TSharedPtr<FJsonObject> Camera = GatewayCameras[0]->AsObject();
			TestEqual(TEXT("网关相机索引 165-0-7"), TEXT("165-0-7"), Camera->GetStringField(TEXT("camera_index")));
			const TArray<TSharedPtr<FJsonValue>> Videos = Camera->GetArrayField(TEXT("video_list"));
			TestEqual(TEXT("网关相机 video_list 含 1 项"), 1, Videos.Num());
			if (Videos.Num() > 0)
			{
				const TSharedPtr<FJsonObject> Video = Videos[0]->AsObject();
				TestEqual(TEXT("网关视频 video_index=normal-0"), TEXT("normal-0"), Video->GetStringField(TEXT("video_index")));
				TestEqual(TEXT("网关视频 video_type=normal"), TEXT("normal"), Video->GetStringField(TEXT("video_type")));
			}
		}
	}

	// 无人机设备项：176-0-0 普通相机 + 52-0-0 主载荷（zoom）
	const TSharedPtr<FJsonObject> Drone = DeviceList[1]->AsObject();
	TestTrue(TEXT("无人机设备项可解析"), Drone.IsValid());
	if (Drone.IsValid())
	{
		TestEqual(TEXT("无人机设备 sn=无人机SN"), Bridge->DroneSn, Drone->GetStringField(TEXT("sn")));
		TestEqual(TEXT("无人机 available_video_number=2"), 2.0, Drone->GetNumberField(TEXT("available_video_number")));
		const TArray<TSharedPtr<FJsonValue>> DroneCameras = Drone->GetArrayField(TEXT("camera_list"));
		TestEqual(TEXT("无人机 camera_list 含 2 项"), 2, DroneCameras.Num());
		if (DroneCameras.Num() == 2)
		{
			const TSharedPtr<FJsonObject> NormalCam = DroneCameras[0]->AsObject();
			TestEqual(TEXT("普通相机索引 176-0-0"), TEXT("176-0-0"), NormalCam->GetStringField(TEXT("camera_index")));

			const TSharedPtr<FJsonObject> PayloadCam = DroneCameras[1]->AsObject();
			TestEqual(TEXT("主载荷相机索引=相机索引"), Bridge->CameraIndex, PayloadCam->GetStringField(TEXT("camera_index")));
			const TArray<TSharedPtr<FJsonValue>> PayloadVideos = PayloadCam->GetArrayField(TEXT("video_list"));
			TestEqual(TEXT("主载荷 video_list 含 1 项"), 1, PayloadVideos.Num());
			if (PayloadVideos.Num() > 0)
			{
				const TSharedPtr<FJsonObject> Video = PayloadVideos[0]->AsObject();
				TestEqual(TEXT("主载荷 video_type=zoom"), TEXT("zoom"), Video->GetStringField(TEXT("video_type")));
				const TArray<TSharedPtr<FJsonValue>> Switchable = Video->GetArrayField(TEXT("switchable_video_types"));
				TestEqual(TEXT("可切换类型 4 项"), 4, Switchable.Num());
				if (Switchable.Num() == 4)
				{
					TestEqual(TEXT("可切换类型[0]=normal"), TEXT("normal"), Switchable[0]->AsString());
					TestEqual(TEXT("可切换类型[1]=wide"), TEXT("wide"), Switchable[1]->AsString());
					TestEqual(TEXT("可切换类型[2]=zoom"), TEXT("zoom"), Switchable[2]->AsString());
					TestEqual(TEXT("可切换类型[3]=ir"), TEXT("ir"), Switchable[3]->AsString());
				}
			}
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
