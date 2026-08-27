// 指点飞行自动化测试：fly_to_point_progress 事件结构 + fly_to_point 指令语义
#include "Misc/AutomationTest.h"
#include "UAVMqttBridgeComponent.h"
#include "UAVDroneSimComponent.h"
#include "UAVFlightControlComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** 构造合法的 fly_to_point 指令 data（可覆盖 max_speed / height 便于参数校验用例） */
	FString MakeFlyToPointData(const FString& InFlyToId, double InMaxSpeed, double InHeight)
	{
		return FString::Printf(TEXT("{\"fly_to_id\":\"%s\",\"max_speed\":%f,\"points\":[{\"latitude\":30.123456,\"longitude\":120.654321,\"height\":%f}]}"),
			*InFlyToId, InMaxSpeed, InHeight);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVFlyToPointEventDataTest, "UAV.MqttBridge.Events.FlyToPointProgress", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVFlyToPointEventDataTest::RunTest(const FString& Parameters)
{
	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> Data = Bridge->BuildFlyToPointProgressEventData(TEXT("wayline_progress"), TEXT("FLY001"), 0, 0);
	TestTrue(TEXT("fly_to_point_progress data 已组装"), Data.IsValid());
	if (!Data.IsValid())
	{
		return false;
	}
	TestEqual(TEXT("result=0"), 0.0, Data->GetNumberField(TEXT("result")));
	TestEqual(TEXT("status=wayline_progress"), TEXT("wayline_progress"), Data->GetStringField(TEXT("status")));
	TestEqual(TEXT("fly_to_id=FLY001"), TEXT("FLY001"), Data->GetStringField(TEXT("fly_to_id")));
	TestEqual(TEXT("way_point_index=0（数字）"), 0.0, Data->GetNumberField(TEXT("way_point_index")));
	TestFalse(TEXT("不含飞行任务字段 flight_id"), Data->HasField(TEXT("flight_id")));

	// 完成/取消状态透传
	const TSharedPtr<FJsonObject> Ok = Bridge->BuildFlyToPointProgressEventData(TEXT("wayline_ok"), TEXT("FLY001"), 1, 0);
	TestEqual(TEXT("wayline_ok 状态透传"), TEXT("wayline_ok"), Ok->GetStringField(TEXT("status")));
	TestEqual(TEXT("wayline_ok 航点索引透传"), 1.0, Ok->GetNumberField(TEXT("way_point_index")));
	const TSharedPtr<FJsonObject> Cancel = Bridge->BuildFlyToPointProgressEventData(TEXT("wayline_cancel"), TEXT("FLY001"), 0, 0);
	TestEqual(TEXT("wayline_cancel 状态透传"), TEXT("wayline_cancel"), Cancel->GetStringField(TEXT("status")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVFlyToPointCommandTest, "UAV.FlightControl.FlyToPoint", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVFlyToPointCommandTest::RunTest(const FString& Parameters)
{
	using namespace UAV::FlightControlResult;

	UUAVDroneSimComponent* Sim = NewObject<UUAVDroneSimComponent>();
	TestNotNull(TEXT("模拟组件可创建"), Sim);
	if (!Sim)
	{
		return false;
	}

	// 未抢占飞行权：fly_to_point / fly_to_point_stop / fly_to_point_update 均返回 NoAuthority
	{
		UUAVFlightControlComponent* NoAuth = NewObject<UUAVFlightControlComponent>();
		NoAuth->SetDroneSim(Sim);
		TestEqual(TEXT("无飞控权 → NoAuthority"),
			NoAuthority, NoAuth->HandleCommand(TEXT("fly_to_point"), MakeFlyToPointData(TEXT("FLY001"), 8.0, 100.0)));
		TestEqual(TEXT("无飞控权 stop → NoAuthority"),
			NoAuthority, NoAuth->HandleCommand(TEXT("fly_to_point_stop"), TEXT("")));
		TestEqual(TEXT("无飞控权 update → NoAuthority"),
			NoAuthority, NoAuth->HandleCommand(TEXT("fly_to_point_update"), MakeFlyToPointData(TEXT(""), 8.0, 100.0)));
	}

	// 参数校验：max_speed 越界 / height 越界 / 目标点缺失
	{
		UUAVFlightControlComponent* Flight = NewObject<UUAVFlightControlComponent>();
		Flight->SetDroneSim(Sim);
		TestEqual(TEXT("抢占飞行权"), Success, Flight->HandleCommand(TEXT("flight_authority_grab"), TEXT("")));
		TestEqual(TEXT("max_speed=16 越界 → InvalidParams"),
			InvalidParams, Flight->HandleCommand(TEXT("fly_to_point"), MakeFlyToPointData(TEXT("FLY001"), 16.0, 100.0)));
		TestEqual(TEXT("height=1 低于下限 → InvalidParams"),
			InvalidParams, Flight->HandleCommand(TEXT("fly_to_point"), MakeFlyToPointData(TEXT("FLY001"), 8.0, 1.0)));
		TestEqual(TEXT("缺少 fly_to_id → InvalidParams"),
			InvalidParams, Flight->HandleCommand(TEXT("fly_to_point"), MakeFlyToPointData(TEXT(""), 8.0, 100.0)));
	}

	// 正常流程：启动 → 状态迁移 Flying；stop 停止；update 复用会话
	{
		UUAVFlightControlComponent* Flight = NewObject<UUAVFlightControlComponent>();
		Flight->SetDroneSim(Sim);
		TestEqual(TEXT("抢占飞行权"), Success, Flight->HandleCommand(TEXT("flight_authority_grab"), TEXT("")));

		TestEqual(TEXT("未在指点会话中 stop → StateConflict"),
			StateConflict, Flight->HandleCommand(TEXT("fly_to_point_stop"), TEXT("")));
		TestEqual(TEXT("未在指点会话中 update → StateConflict"),
			StateConflict, Flight->HandleCommand(TEXT("fly_to_point_update"), MakeFlyToPointData(TEXT(""), 8.0, 100.0)));

		TestEqual(TEXT("fly_to_point 启动 → Success"),
			Success, Flight->HandleCommand(TEXT("fly_to_point"), MakeFlyToPointData(TEXT("FLY001"), 8.0, 100.0)));
		TestEqual(TEXT("启动后进入空中巡航状态"),
			static_cast<int32>(EUAVFlightState::Flying), static_cast<int32>(Flight->GetFlightState()));

		TestEqual(TEXT("fly_to_point_update 更新目标 → Success"),
			Success, Flight->HandleCommand(TEXT("fly_to_point_update"), MakeFlyToPointData(TEXT(""), 10.0, 120.0)));
		TestEqual(TEXT("更新后保持巡航状态"),
			static_cast<int32>(EUAVFlightState::Flying), static_cast<int32>(Flight->GetFlightState()));

		TestEqual(TEXT("fly_to_point_stop 停止 → Success"),
			Success, Flight->HandleCommand(TEXT("fly_to_point_stop"), TEXT("")));
	}

	// 状态冲突：航线任务执行中（Wayline）不允许启动指点飞行
	{
		UUAVFlightControlComponent* Flight = NewObject<UUAVFlightControlComponent>();
		Flight->SetDroneSim(Sim);
		TestEqual(TEXT("抢占飞行权"), Success, Flight->HandleCommand(TEXT("flight_authority_grab"), TEXT("")));
		TestEqual(TEXT("航线任务登记"), Success, Flight->HandleCommand(TEXT("flighttask_create"), TEXT("{\"flight_id\":\"FLT-1\"}")));
		TestEqual(TEXT("航线任务就绪"), Success, Flight->HandleCommand(TEXT("flighttask_prepare"), TEXT("{\"flight_id\":\"FLT-1\"}")));
		TestEqual(TEXT("航线任务执行"), Success, Flight->HandleCommand(TEXT("flighttask_execute"), TEXT("{\"flight_id\":\"FLT-1\"}")));
		TestEqual(TEXT("执行后进入航线状态"),
			static_cast<int32>(EUAVFlightState::Wayline), static_cast<int32>(Flight->GetFlightState()));
		TestEqual(TEXT("航线执行中启动指点飞行 → StateConflict"),
			StateConflict, Flight->HandleCommand(TEXT("fly_to_point"), MakeFlyToPointData(TEXT("FLY001"), 8.0, 100.0)));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
