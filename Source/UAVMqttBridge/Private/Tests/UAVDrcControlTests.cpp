// DRC 摇杆直控自动化测试：drc_status_notify 事件结构 + drc/up 回执结构 + DRC 会话状态机
#include "Misc/AutomationTest.h"
#include "UAVMqttBridgeComponent.h"
#include "UAVDroneSimComponent.h"
#include "UAVFlightControlComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** 构造合法的 fly_to_point 指令 data（用于让无人机进入空中状态） */
	FString MakeFlyToPointData(const FString& InFlyToId, double InMaxSpeed, double InHeight)
	{
		return FString::Printf(TEXT("{\"fly_to_id\":\"%s\",\"max_speed\":%f,\"points\":[{\"latitude\":30.123456,\"longitude\":120.654321,\"height\":%f}]}"),
			*InFlyToId, InMaxSpeed, InHeight);
	}

	/** 构造 drone_control 指令 data */
	FString MakeDroneControlData(int32 InSeq, int32 InX, int32 InY, int32 InH, int32 InW, int32 InFreq, int32 InDelayTime)
	{
		return FString::Printf(TEXT("{\"seq\":%d,\"x\":%d,\"y\":%d,\"h\":%d,\"w\":%d,\"freq\":%d,\"delayTime\":%d}"),
			InSeq, InX, InY, InH, InW, InFreq, InDelayTime);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVDrcStatusNotifyEventTest, "UAV.MqttBridge.Events.DrcStatusNotify", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVDrcStatusNotifyEventTest::RunTest(const FString& Parameters)
{
	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	// drc_status_notify 事件 data：{result:0, drc_state}
	const TSharedPtr<FJsonObject> Connected = Bridge->BuildDrcStatusNotifyData(2);
	TestTrue(TEXT("drc_status_notify data 已组装"), Connected.IsValid());
	if (Connected.IsValid())
	{
		TestEqual(TEXT("result=0"), 0.0, Connected->GetNumberField(TEXT("result")));
		TestEqual(TEXT("drc_state=2（已连接）"), 2.0, Connected->GetNumberField(TEXT("drc_state")));
	}
	const TSharedPtr<FJsonObject> Disconnected = Bridge->BuildDrcStatusNotifyData(0);
	TestEqual(TEXT("drc_state=0（断开）"), 0.0, Disconnected->GetNumberField(TEXT("drc_state")));

	// drone_control 回执：data={result, output:{seq}}
	const TSharedPtr<FJsonObject> Control = Bridge->BuildDrcUpReply(TEXT("drone_control"), TEXT("tid-1"), TEXT("bid-1"), 0, 42);
	TestTrue(TEXT("drc/up 回执已组装"), Control.IsValid());
	if (Control.IsValid())
	{
		TestEqual(TEXT("回执 tid 透传"), TEXT("tid-1"), Control->GetStringField(TEXT("tid")));
		TestEqual(TEXT("回执 bid 透传"), TEXT("bid-1"), Control->GetStringField(TEXT("bid")));
		TestEqual(TEXT("回执 method 透传"), TEXT("drone_control"), Control->GetStringField(TEXT("method")));
		const TSharedPtr<FJsonObject> Data = Control->GetObjectField(TEXT("data"));
		TestTrue(TEXT("回执 data 对象存在"), Data.IsValid());
		if (Data.IsValid())
		{
			TestEqual(TEXT("回执 result=0"), 0.0, Data->GetNumberField(TEXT("result")));
			const TSharedPtr<FJsonObject> Output = Data->GetObjectField(TEXT("output"));
			TestTrue(TEXT("drone_control 回执带 output"), Output.IsValid());
			if (Output.IsValid())
			{
				TestEqual(TEXT("output.seq=42"), 42.0, Output->GetNumberField(TEXT("seq")));
			}
		}
	}

	// 急停回执：仅 result，无 output
	const TSharedPtr<FJsonObject> Stop = Bridge->BuildDrcUpReply(TEXT("drone_emergency_stop"), TEXT("tid-2"), TEXT("bid-2"), 0, -1);
	const TSharedPtr<FJsonObject> StopData = Stop->GetObjectField(TEXT("data"));
	TestTrue(TEXT("急停回执 data 对象存在"), StopData.IsValid());
	if (StopData.IsValid())
	{
		TestEqual(TEXT("急停回执 result=0"), 0.0, StopData->GetNumberField(TEXT("result")));
		TestFalse(TEXT("急停回执不含 output"), StopData->HasField(TEXT("output")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVFlightControlDrcSessionTest, "UAV.FlightControl.DrcSession", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVFlightControlDrcSessionTest::RunTest(const FString& Parameters)
{
	using namespace UAV::FlightControlResult;

	UUAVDroneSimComponent* Sim = NewObject<UUAVDroneSimComponent>();
	TestNotNull(TEXT("模拟组件可创建"), Sim);
	if (!Sim)
	{
		return false;
	}

	// 未抢占飞行权：drc_mode_enter → NoAuthority
	{
		UUAVFlightControlComponent* NoAuth = NewObject<UUAVFlightControlComponent>();
		NoAuth->SetDroneSim(Sim);
		TestEqual(TEXT("无飞控权进入 DRC → NoAuthority"),
			NoAuthority, NoAuth->HandleCommand(TEXT("drc_mode_enter"), TEXT("")));
	}

	// 抢占飞行权但待机：drc_mode_enter → StateConflict（需空中状态）
	{
		UUAVFlightControlComponent* Idle = NewObject<UUAVFlightControlComponent>();
		Idle->SetDroneSim(Sim);
		TestEqual(TEXT("抢占飞行权"), Success, Idle->HandleCommand(TEXT("flight_authority_grab"), TEXT("")));
		TestEqual(TEXT("待机状态进入 DRC → StateConflict"),
			StateConflict, Idle->HandleCommand(TEXT("drc_mode_enter"), TEXT("")));
	}

	UUAVFlightControlComponent* Flight = NewObject<UUAVFlightControlComponent>();
	Flight->SetDroneSim(Sim);
	TestEqual(TEXT("抢占飞行权"), Success, Flight->HandleCommand(TEXT("flight_authority_grab"), TEXT("")));
	TestEqual(TEXT("起飞到点（空中）"), Success,
		Flight->HandleCommand(TEXT("fly_to_point"), MakeFlyToPointData(TEXT("DRC001"), 8.0, 100.0)));
	TestEqual(TEXT("空中状态"), static_cast<int32>(EUAVFlightState::Flying), static_cast<int32>(Flight->GetFlightState()));

	// 非会话状态：drone_control / heart_beat → StateConflict
	TestEqual(TEXT("未进入会话 drone_control → StateConflict"),
		StateConflict, Flight->HandleCommand(TEXT("drone_control"), MakeDroneControlData(1, 0, 0, 0, 0, 5, 500)));
	TestEqual(TEXT("未进入会话 heart_beat → StateConflict"),
		StateConflict, Flight->HandleCommand(TEXT("heart_beat"), TEXT("{\"seq\":1}")));

	// 进入 DRC 会话
	TestEqual(TEXT("进入 DRC 会话 → Success"), Success, Flight->HandleCommand(TEXT("drc_mode_enter"), TEXT("")));

	// 参数校验：越界 / 缺 seq
	TestEqual(TEXT("x=18 越界 → InvalidParams"),
		InvalidParams, Flight->HandleCommand(TEXT("drone_control"), MakeDroneControlData(2, 18, 0, 0, 0, 5, 500)));
	TestEqual(TEXT("h=-5 越界 → InvalidParams"),
		InvalidParams, Flight->HandleCommand(TEXT("drone_control"), MakeDroneControlData(2, 0, 0, -5, 0, 5, 500)));
	TestEqual(TEXT("w=91 越界 → InvalidParams"),
		InvalidParams, Flight->HandleCommand(TEXT("drone_control"), MakeDroneControlData(2, 0, 0, 0, 91, 5, 500)));
	TestEqual(TEXT("freq=1 越界 → InvalidParams"),
		InvalidParams, Flight->HandleCommand(TEXT("drone_control"), MakeDroneControlData(2, 0, 0, 0, 0, 1, 500)));
	TestEqual(TEXT("delayTime=50 越界 → InvalidParams"),
		InvalidParams, Flight->HandleCommand(TEXT("drone_control"), MakeDroneControlData(2, 0, 0, 0, 0, 5, 50)));
	TestEqual(TEXT("缺 seq → InvalidParams"),
		InvalidParams, Flight->HandleCommand(TEXT("drone_control"), TEXT("{\"x\":0,\"y\":0,\"h\":0,\"w\":0,\"freq\":5,\"delayTime\":500}")));

	// 合法摇杆控制：停止任务、激活摇杆、记录 seq
	TestEqual(TEXT("drone_control 合法 → Success"), Success,
		Flight->HandleCommand(TEXT("drone_control"), MakeDroneControlData(7, 3, -2, 1, 30, 5, 500)));
	TestEqual(TEXT("记录最近 seq"), 7, Flight->GetLastDrcSeq());
	TestTrue(TEXT("摇杆控制已激活"), Sim->IsJoystickActive());
	TestFalse(TEXT("航点任务已停止（互斥）"), Sim->HasActiveMission());

	// 心跳：记录 seq 并回执
	TestEqual(TEXT("heart_beat → Success"), Success, Flight->HandleCommand(TEXT("heart_beat"), TEXT("{\"seq\":8}")));
	TestEqual(TEXT("心跳更新最近 seq"), 8, Flight->GetLastDrcSeq());
	TestEqual(TEXT("心跳缺 seq → InvalidParams"),
		InvalidParams, Flight->HandleCommand(TEXT("heart_beat"), TEXT("{}")));

	// 急停：停止一切运动，保持会话
	TestEqual(TEXT("drone_emergency_stop → Success"), Success, Flight->HandleCommand(TEXT("drone_emergency_stop"), TEXT("")));
	TestFalse(TEXT("急停后摇杆停用"), Sim->IsJoystickActive());
	TestEqual(TEXT("急停后仍可 drone_control（会话保持）"), Success,
		Flight->HandleCommand(TEXT("drone_control"), MakeDroneControlData(9, 0, 0, 0, 0, 5, 500)));

	// 退出会话：停止摇杆控制，重复退出 → StateConflict
	TestEqual(TEXT("drc_mode_exit → Success"), Success, Flight->HandleCommand(TEXT("drc_mode_exit"), TEXT("")));
	TestFalse(TEXT("退出后摇杆停用"), Sim->IsJoystickActive());
	TestEqual(TEXT("退出后 drone_control → StateConflict"),
		StateConflict, Flight->HandleCommand(TEXT("drone_control"), MakeDroneControlData(10, 0, 0, 0, 0, 5, 500)));
	TestEqual(TEXT("重复退出 → StateConflict"),
		StateConflict, Flight->HandleCommand(TEXT("drc_mode_exit"), TEXT("")));

	// 退出后可重新进入（空中状态保持）
	TestEqual(TEXT("重新进入 DRC → Success"), Success, Flight->HandleCommand(TEXT("drc_mode_enter"), TEXT("")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
