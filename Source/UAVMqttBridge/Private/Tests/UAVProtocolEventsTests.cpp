// 上云 API 事件报文结构自动化测试：flighttask_progress / return_home_info / flighttask_ready / hms
#include "Misc/AutomationTest.h"
#include "UAVMqttBridgeComponent.h"
#include "UAVDroneSimComponent.h"
#include "UAVFlightControlComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVFlighttaskProgressEventTest, "UAV.MqttBridge.Events.FlighttaskProgress", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVFlighttaskProgressEventTest::RunTest(const FString& Parameters)
{
	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	// in_progress：wayline_mission_state=6（执行中）
	const TSharedPtr<FJsonObject> Data = Bridge->BuildFlighttaskProgressEventData(TEXT("in_progress"), TEXT("FLT-001"), 2, 45);
	TestTrue(TEXT("flighttask_progress data 已组装"), Data.IsValid());
	if (!Data.IsValid())
	{
		return false;
	}

	TestEqual(TEXT("result=0"), 0.0, Data->GetNumberField(TEXT("result")));
	const TSharedPtr<FJsonObject> Output = Data->GetObjectField(TEXT("output"));
	TestTrue(TEXT("output 必填且非 null"), Output.IsValid());
	if (!Output.IsValid())
	{
		return false;
	}
	TestEqual(TEXT("output.status=in_progress"), TEXT("in_progress"), Output->GetStringField(TEXT("status")));

	const TSharedPtr<FJsonObject> Progress = Output->GetObjectField(TEXT("progress"));
	TestTrue(TEXT("progress 对象存在"), Progress.IsValid());
	if (Progress.IsValid())
	{
		TestEqual(TEXT("progress.current_step=2"), 2.0, Progress->GetNumberField(TEXT("current_step")));
		TestEqual(TEXT("progress.percent=45"), 45.0, Progress->GetNumberField(TEXT("percent")));
	}

	const TSharedPtr<FJsonObject> Ext = Output->GetObjectField(TEXT("ext"));
	TestTrue(TEXT("ext 对象存在"), Ext.IsValid());
	if (Ext.IsValid())
	{
		TestEqual(TEXT("ext.current_waypoint_index=2"), 2.0, Ext->GetNumberField(TEXT("current_waypoint_index")));
		TestEqual(TEXT("ext.media_count=0"), 0.0, Ext->GetNumberField(TEXT("media_count")));
		TestEqual(TEXT("ext.flight_id=FLT-001"), TEXT("FLT-001"), Ext->GetStringField(TEXT("flight_id")));
		TestEqual(TEXT("ext.wayline_id=W000000001"), TEXT("W000000001"), Ext->GetStringField(TEXT("wayline_id")));
		TestEqual(TEXT("ext.wayline_mission_state=6"), 6.0, Ext->GetNumberField(TEXT("wayline_mission_state")));
		TestTrue(TEXT("ext.track_id 非空"), !Ext->GetStringField(TEXT("track_id")).IsEmpty());
	}

	// sent=5（到达首航点）、终态 ok=9（结束）
	const TSharedPtr<FJsonObject> Sent = Bridge->BuildFlighttaskProgressEventData(TEXT("sent"), TEXT("FLT-001"), 1, 10);
	TestEqual(TEXT("sent → wayline_mission_state=5"),
		5.0, Sent->GetObjectField(TEXT("output"))->GetObjectField(TEXT("ext"))->GetNumberField(TEXT("wayline_mission_state")));
	const TSharedPtr<FJsonObject> Ok = Bridge->BuildFlighttaskProgressEventData(TEXT("ok"), TEXT("FLT-001"), 4, 100);
	TestEqual(TEXT("ok → wayline_mission_state=9"),
		9.0, Ok->GetObjectField(TEXT("output"))->GetObjectField(TEXT("ext"))->GetNumberField(TEXT("wayline_mission_state")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVReturnHomeInfoEventTest, "UAV.MqttBridge.Events.ReturnHomeInfo", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVReturnHomeInfoEventTest::RunTest(const FString& Parameters)
{
	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	UUAVDroneSimComponent* Sim = NewObject<UUAVDroneSimComponent>();
	UUAVFlightControlComponent* FlightControl = NewObject<UUAVFlightControlComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	TestNotNull(TEXT("模拟组件可创建"), Sim);
	TestNotNull(TEXT("飞控组件可创建"), FlightControl);
	if (!Bridge || !Sim || !FlightControl)
	{
		return false;
	}
	Sim->AirportOrigin.Latitude = 30.123456;
	Sim->AirportOrigin.Longitude = 120.654321;
	Bridge->SetDroneSim(Sim);
	Bridge->SetFlightControl(FlightControl);

	const TSharedPtr<FJsonObject> Data = Bridge->BuildReturnHomeInfoEventData();
	TestTrue(TEXT("return_home_info data 已组装"), Data.IsValid());
	if (!Data.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>> PathPoints = Data->GetArrayField(TEXT("planned_path_points"));
	TestEqual(TEXT("planned_path_points 含 1 个返航点"), 1, PathPoints.Num());
	if (PathPoints.Num() > 0)
	{
		const TSharedPtr<FJsonObject> Point = PathPoints[0]->AsObject();
		TestTrue(TEXT("返航点含 latitude"), Point.IsValid() && Point->HasField(TEXT("latitude")));
		TestTrue(TEXT("返航点含 longitude"), Point.IsValid() && Point->HasField(TEXT("longitude")));
		TestTrue(TEXT("返航点含 height"), Point.IsValid() && Point->HasField(TEXT("height")));
	}
	TestEqual(TEXT("last_point_type=0（直飞返航点）"), 0.0, Data->GetNumberField(TEXT("last_point_type")));
	TestEqual(TEXT("flight_id 存在"), TEXT(""), Data->GetStringField(TEXT("flight_id")));

	// 不再包含旧 return_home_status 平铺字段
	TestFalse(TEXT("不含旧字段 status"), Data->HasField(TEXT("status")));
	TestFalse(TEXT("不含旧字段 reason"), Data->HasField(TEXT("reason")));

	// 未注入 DroneSim 时：无返航点，last_point_type=65535
	UUAVMqttBridgeComponent* BareBridge = NewObject<UUAVMqttBridgeComponent>();
	const TSharedPtr<FJsonObject> BareData = BareBridge->BuildReturnHomeInfoEventData();
	TestEqual(TEXT("无 DroneSim 时 planned_path_points 为空"), 0, BareData->GetArrayField(TEXT("planned_path_points")).Num());
	TestEqual(TEXT("无 DroneSim 时 last_point_type=65535"), 65535.0, BareData->GetNumberField(TEXT("last_point_type")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVFlighttaskReadyEventTest, "UAV.MqttBridge.Events.FlighttaskReady", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVFlighttaskReadyEventTest::RunTest(const FString& Parameters)
{
	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> Data = Bridge->BuildFlighttaskReadyData(TEXT("FLT-001"));
	TestTrue(TEXT("flighttask_ready data 已组装"), Data.IsValid());
	if (!Data.IsValid())
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>> FlightIds = Data->GetArrayField(TEXT("flight_ids"));
	TestEqual(TEXT("flight_ids 含 1 项"), 1, FlightIds.Num());
	if (FlightIds.Num() > 0)
	{
		TestEqual(TEXT("flight_ids[0]=FLT-001"), TEXT("FLT-001"), FlightIds[0]->AsString());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVHmsEventTest, "UAV.MqttBridge.Events.Hms", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVHmsEventTest::RunTest(const FString& Parameters)
{
	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	// 连接成功后空告警
	const TSharedPtr<FJsonObject> EmptyData = Bridge->BuildHmsPayload();
	TestTrue(TEXT("hms 空告警 data 已组装"), EmptyData.IsValid());
	TestEqual(TEXT("空告警 list 为空"), 0, EmptyData->GetArrayField(TEXT("list")).Num());

	// 低电量告警
	const TSharedPtr<FJsonObject> AlarmData = Bridge->BuildHmsPayload(true);
	const TArray<TSharedPtr<FJsonValue>> List = AlarmData->GetArrayField(TEXT("list"));
	TestEqual(TEXT("低电量告警 list 含 1 项"), 1, List.Num());
	if (List.Num() > 0)
	{
		const TSharedPtr<FJsonObject> Alarm = List[0]->AsObject();
		TestEqual(TEXT("code=fpv_tip_0x1B030014"), TEXT("fpv_tip_0x1B030014"), Alarm->GetStringField(TEXT("code")));
		TestEqual(TEXT("device_type=0-100-1"), TEXT("0-100-1"), Alarm->GetStringField(TEXT("device_type")));
		TestTrue(TEXT("imminent=true"), Alarm->GetBoolField(TEXT("imminent")));
		TestTrue(TEXT("in_the_sky=true"), Alarm->GetBoolField(TEXT("in_the_sky")));
		TestEqual(TEXT("level=1"), 1.0, Alarm->GetNumberField(TEXT("level")));
		TestEqual(TEXT("module=0"), 0.0, Alarm->GetNumberField(TEXT("module")));
		const TSharedPtr<FJsonObject> Args = Alarm->GetObjectField(TEXT("args"));
		TestTrue(TEXT("args 对象存在"), Args.IsValid());
		if (Args.IsValid())
		{
			TestEqual(TEXT("args.component_index=0"), 0.0, Args->GetNumberField(TEXT("component_index")));
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
