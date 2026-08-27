// 机场 OSD 自动化测试：结构完整性 + 状态推导（任务 3.x）
#include "Misc/AutomationTest.h"
#include "UAVMqttBridgeComponent.h"
#include "UAVDroneSimComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** 构造带机场原点的模拟组件（当前位于机场原点、待机、满电） */
	UUAVDroneSimComponent* MakeDockedDroneSim()
	{
		UUAVDroneSimComponent* Sim = NewObject<UUAVDroneSimComponent>();
		Sim->AirportOrigin.Latitude = 30.123456;
		Sim->AirportOrigin.Longitude = 120.654321;
		Sim->AirportOrigin.Altitude = 10.0;
		Sim->SetFlightState(EUAVFlightState::Idle);
		return Sim;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVDockOsdStructureTest, "UAV.MqttBridge.DockOsd.Structure", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVDockOsdStructureTest::RunTest(const FString& Parameters)
{
	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	UUAVDroneSimComponent* Sim = MakeDockedDroneSim();
	TestNotNull(TEXT("模拟组件可创建"), Sim);
	if (!Bridge || !Sim)
	{
		return false;
	}
	Bridge->SetDroneSim(Sim);

	const TSharedPtr<FJsonObject> Data = Bridge->BuildDockOsdPayload();
	TestTrue(TEXT("机场 OSD data 已组装"), Data.IsValid());
	if (!Data.IsValid())
	{
		return false;
	}

	// 顶层字段集（对齐 dock OsdDock）
	const TArray<FString> RequiredFields = {
		TEXT("network_state"), TEXT("drone_in_dock"), TEXT("drone_charge_state"),
		TEXT("rainfall"), TEXT("wind_speed"), TEXT("environment_temperature"),
		TEXT("temperature"), TEXT("humidity"), TEXT("latitude"), TEXT("longitude"),
		TEXT("height"), TEXT("alternate_land_point"), TEXT("first_power_on"),
		TEXT("position_state"), TEXT("storage"), TEXT("mode_code"), TEXT("cover_state"),
		TEXT("supplement_light_state"), TEXT("emergency_stop_state"), TEXT("air_conditioner"),
		TEXT("battery_store_mode"), TEXT("alarm_state"), TEXT("putter_state"),
		TEXT("sub_device"), TEXT("job_number"), TEXT("acc_time"), TEXT("activation_time"),
		TEXT("electric_supply_voltage"), TEXT("working_voltage"), TEXT("working_current"),
		TEXT("backup_battery"), TEXT("drone_battery_maintenance_info"),
		TEXT("flighttask_step_code"), TEXT("flighttask_prepare_capacity"),
		TEXT("media_file_detail"), TEXT("wireless_link"), TEXT("drc_state"),
		TEXT("user_experience_improvement"),
	};
	for (const FString& Field : RequiredFields)
	{
		TestTrue(FString::Printf(TEXT("包含字段 %s"), *Field), Data->HasField(Field));
	}

	// 子对象结构完整
	TestTrue(TEXT("alternate_land_point 含 safe_land_height"),
		Data->GetObjectField(TEXT("alternate_land_point"))->HasField(TEXT("safe_land_height")));
	TestTrue(TEXT("alternate_land_point 含 is_configured"),
		Data->GetObjectField(TEXT("alternate_land_point"))->HasField(TEXT("is_configured")));
	TestTrue(TEXT("position_state 含 gps_number"),
		Data->GetObjectField(TEXT("position_state"))->HasField(TEXT("gps_number")));
	TestTrue(TEXT("storage 含 total/used"),
		Data->GetObjectField(TEXT("storage"))->HasField(TEXT("total"))
		&& Data->GetObjectField(TEXT("storage"))->HasField(TEXT("used")));
	TestTrue(TEXT("air_conditioner 含 air_conditioner_state/switch_time"),
		Data->GetObjectField(TEXT("air_conditioner"))->HasField(TEXT("air_conditioner_state"))
		&& Data->GetObjectField(TEXT("air_conditioner"))->HasField(TEXT("switch_time")));
	TestTrue(TEXT("sub_device 含 device_sn/device_model_key"),
		Data->GetObjectField(TEXT("sub_device"))->HasField(TEXT("device_sn"))
		&& Data->GetObjectField(TEXT("sub_device"))->HasField(TEXT("device_model_key")));
	TestTrue(TEXT("backup_battery 含 voltage/temperature/switch"),
		Data->GetObjectField(TEXT("backup_battery"))->HasField(TEXT("voltage"))
		&& Data->GetObjectField(TEXT("backup_battery"))->HasField(TEXT("switch")));
	TestTrue(TEXT("drone_battery_maintenance_info 含 maintenance_state"),
		Data->GetObjectField(TEXT("drone_battery_maintenance_info"))->HasField(TEXT("maintenance_state")));
	TestTrue(TEXT("media_file_detail 含 remain_upload"),
		Data->GetObjectField(TEXT("media_file_detail"))->HasField(TEXT("remain_upload")));
	TestTrue(TEXT("wireless_link 含 4g_link_state/sdr_link_state"),
		Data->GetObjectField(TEXT("wireless_link"))->HasField(TEXT("4g_link_state"))
		&& Data->GetObjectField(TEXT("wireless_link"))->HasField(TEXT("sdr_link_state")));
	TestTrue(TEXT("drone_charge_state 含 capacity_percent/state"),
		Data->GetObjectField(TEXT("drone_charge_state"))->HasField(TEXT("capacity_percent"))
		&& Data->GetObjectField(TEXT("drone_charge_state"))->HasField(TEXT("state")));
	TestTrue(TEXT("network_state 含 type/quality/rate"),
		Data->GetObjectField(TEXT("network_state"))->HasField(TEXT("type"))
		&& Data->GetObjectField(TEXT("network_state"))->HasField(TEXT("rate")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVDockOsdDockedTest, "UAV.MqttBridge.DockOsd.Docked", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVDockOsdDockedTest::RunTest(const FString& Parameters)
{
	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	UUAVDroneSimComponent* Sim = MakeDockedDroneSim();
	if (!Bridge || !Sim)
	{
		return false;
	}
	Bridge->SetDroneSim(Sim);
	const TSharedPtr<FJsonObject> Data = Bridge->BuildDockOsdPayload();
	if (!Data.IsValid())
	{
		return false;
	}

	// 归巢待命：mode_code=3、cover_state=0、drone_in_dock=true
	TestTrue(TEXT("待机在机场内 drone_in_dock=true"), Data->GetBoolField(TEXT("drone_in_dock")));
	TestEqual(TEXT("待机在机场内 mode_code=3"), 3.0, Data->GetNumberField(TEXT("mode_code")));
	TestEqual(TEXT("待机在机场内 cover_state=0"), 0.0, Data->GetNumberField(TEXT("cover_state")));
	TestEqual(TEXT("待机 flighttask_step_code=5"), 5.0, Data->GetNumberField(TEXT("flighttask_step_code")));

	// 充电状态：模拟耗电至 60 后 < 100 → state=1（先验证满电不充电）
	const TSharedPtr<FJsonObject> FullCharge = Data->GetObjectField(TEXT("drone_charge_state"));
	TestEqual(TEXT("满电 state=0"), 0.0, FullCharge->GetNumberField(TEXT("state")));
	Sim->SetBatteryCapacityPercent(60.0);
	TestTrue(TEXT("电量已设为 60"), FMath::IsNearlyEqual(Sim->GetBatteryCapacityPercent(), 60.0, 0.5));
	const TSharedPtr<FJsonObject> LowData = Bridge->BuildDockOsdPayload();
	TestEqual(TEXT("待机充电中 state=1"), 1.0, LowData->GetObjectField(TEXT("drone_charge_state"))->GetNumberField(TEXT("state")));
	TestEqual(TEXT("电量取整 60"), 60.0, LowData->GetObjectField(TEXT("drone_charge_state"))->GetNumberField(TEXT("capacity_percent")));

	// 机场位置与备降点
	TestEqual(TEXT("latitude=机场纬度"), 30.123456, Data->GetNumberField(TEXT("latitude")), 1e-9);
	TestEqual(TEXT("longitude=机场经度"), 120.654321, Data->GetNumberField(TEXT("longitude")), 1e-9);
	TestEqual(TEXT("height=12.0"), 12.0, Data->GetNumberField(TEXT("height")), 1e-9);
	const TSharedPtr<FJsonObject> Alternate = Data->GetObjectField(TEXT("alternate_land_point"));
	TestEqual(TEXT("备降点纬度偏移"), 30.123456 + 0.00012, Alternate->GetNumberField(TEXT("latitude")), 1e-9);
	TestTrue(TEXT("备降点已配置"), Alternate->GetBoolField(TEXT("is_configured")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVDockOsdMissionTest, "UAV.MqttBridge.DockOsd.Mission", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVDockOsdMissionTest::RunTest(const FString& Parameters)
{
	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	UUAVDroneSimComponent* Sim = MakeDockedDroneSim();
	if (!Bridge || !Sim)
	{
		return false;
	}
	Bridge->SetDroneSim(Sim);

	// 任务中：Wayline → mode_code=4、cover_state=1、drone_in_dock=false、step=0
	Sim->SetFlightState(EUAVFlightState::Wayline);
	const TSharedPtr<FJsonObject> WaylineData = Bridge->BuildDockOsdPayload();
	TestFalse(TEXT("任务中 drone_in_dock=false"), WaylineData->GetBoolField(TEXT("drone_in_dock")));
	TestEqual(TEXT("任务中 mode_code=4"), 4.0, WaylineData->GetNumberField(TEXT("mode_code")));
	TestEqual(TEXT("任务中 cover_state=1"), 1.0, WaylineData->GetNumberField(TEXT("cover_state")));
	TestEqual(TEXT("任务中 flighttask_step_code=0"), 0.0, WaylineData->GetNumberField(TEXT("flighttask_step_code")));

	// 返航：step=2
	Sim->SetFlightState(EUAVFlightState::ReturnHome);
	const TSharedPtr<FJsonObject> RthData = Bridge->BuildDockOsdPayload();
	TestEqual(TEXT("返航 flighttask_step_code=2"), 2.0, RthData->GetNumberField(TEXT("flighttask_step_code")));

	// 降落：step=2
	Sim->SetFlightState(EUAVFlightState::Landing);
	const TSharedPtr<FJsonObject> LandingData = Bridge->BuildDockOsdPayload();
	TestEqual(TEXT("降落 flighttask_step_code=2"), 2.0, LandingData->GetNumberField(TEXT("flighttask_step_code")));

	// 空中巡航（Flying）非任务步骤集合：step=5，且不归巢
	Sim->SetFlightState(EUAVFlightState::Flying);
	const TSharedPtr<FJsonObject> FlyingData = Bridge->BuildDockOsdPayload();
	TestEqual(TEXT("巡航 flighttask_step_code=5"), 5.0, FlyingData->GetNumberField(TEXT("flighttask_step_code")));
	TestFalse(TEXT("巡航 drone_in_dock=false"), FlyingData->GetBoolField(TEXT("drone_in_dock")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
