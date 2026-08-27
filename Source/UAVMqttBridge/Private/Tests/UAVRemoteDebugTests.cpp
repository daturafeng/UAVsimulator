// 远程调试指令自动化测试：方法集合精确匹配 + 参数校验 + 进度事件结构 + OSD 状态联动
#include "Misc/AutomationTest.h"
#include "UAVMqttBridgeComponent.h"
#include "UAVDroneSimComponent.h"
#include "UAVFlightControlComponent.h"
#include "UAVCloudApiTypes.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVRemoteDebugMethodSetTest, "UAV.MqttBridge.RemoteDebug.MethodSet", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVRemoteDebugMethodSetTest::RunTest(const FString& Parameters)
{
	// 20 个远程调试/设备控制 method 全部精确命中（对齐 dock DebugMethodEnum，排除 eSIM）
	const TArray<FString> Methods = {
		TEXT("debug_mode_open"), TEXT("debug_mode_close"),
		TEXT("supplement_light_open"), TEXT("supplement_light_close"),
		TEXT("device_reboot"), TEXT("drone_open"), TEXT("drone_close"),
		TEXT("drone_format"), TEXT("device_format"),
		TEXT("cover_open"), TEXT("cover_close"),
		TEXT("putter_open"), TEXT("putter_close"),
		TEXT("charge_open"), TEXT("charge_close"),
		TEXT("battery_maintenance_switch"), TEXT("alarm_state_switch"),
		TEXT("battery_store_mode_switch"), TEXT("sdr_workmode_switch"),
		TEXT("air_conditioner_mode_switch"),
	};
	for (const FString& Method : Methods)
	{
		TestTrue(FString::Printf(TEXT("命中调试指令 %s"), *Method), UUAVMqttBridgeComponent::IsRemoteDebugMethod(Method));
	}

	// 前缀相似的业务指令与 DOCK2 专属 eSIM 指令不得误命中
	TestFalse(TEXT("飞控指令不命中"), UUAVMqttBridgeComponent::IsRemoteDebugMethod(TEXT("flight_authority_grab")));
	TestFalse(TEXT("DRC 指令不命中"), UUAVMqttBridgeComponent::IsRemoteDebugMethod(TEXT("drc_mode_enter")));
	TestFalse(TEXT("eSIM 指令不命中（DOCK2 专属）"), UUAVMqttBridgeComponent::IsRemoteDebugMethod(TEXT("esim_activate")));
	TestFalse(TEXT("未知指令不命中"), UUAVMqttBridgeComponent::IsRemoteDebugMethod(TEXT("cover_toggle")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVRemoteDebugCommandValidationTest, "UAV.MqttBridge.RemoteDebug.CommandValidation", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVRemoteDebugCommandValidationTest::RunTest(const FString& Parameters)
{
	using namespace UAV::FlightControlResult;

	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	// 无参指令：缺省成功
	TestEqual(TEXT("debug_mode_open 成功"), Success, Bridge->HandleDebugCommand(TEXT("debug_mode_open"), TEXT("")));
	TestEqual(TEXT("cover_open 成功"), Success, Bridge->HandleDebugCommand(TEXT("cover_open"), TEXT("")));
	TestEqual(TEXT("device_reboot 成功"), Success, Bridge->HandleDebugCommand(TEXT("device_reboot"), TEXT("")));

	// battery_maintenance_switch：action 仅 0/1
	TestEqual(TEXT("电池保养 action=1 成功"), Success, Bridge->HandleDebugCommand(TEXT("battery_maintenance_switch"), TEXT("{\"action\":1}")));
	TestEqual(TEXT("电池保养 action=0 成功"), Success, Bridge->HandleDebugCommand(TEXT("battery_maintenance_switch"), TEXT("{\"action\":0}")));
	TestEqual(TEXT("电池保养 action=2 越界失败"), InvalidParams, Bridge->HandleDebugCommand(TEXT("battery_maintenance_switch"), TEXT("{\"action\":2}")));
	TestEqual(TEXT("电池保养缺参失败"), InvalidParams, Bridge->HandleDebugCommand(TEXT("battery_maintenance_switch"), TEXT("{}")));

	// alarm_state_switch：action 仅 0/1
	TestEqual(TEXT("报警 action=1 成功"), Success, Bridge->HandleDebugCommand(TEXT("alarm_state_switch"), TEXT("{\"action\":1}")));
	TestEqual(TEXT("报警 action=3 越界失败"), InvalidParams, Bridge->HandleDebugCommand(TEXT("alarm_state_switch"), TEXT("{\"action\":3}")));

	// battery_store_mode_switch：action 仅 1/2（PLAN/EMERGENCY）
	TestEqual(TEXT("存储模式 action=2 成功"), Success, Bridge->HandleDebugCommand(TEXT("battery_store_mode_switch"), TEXT("{\"action\":2}")));
	TestEqual(TEXT("存储模式 action=3 越界失败"), InvalidParams, Bridge->HandleDebugCommand(TEXT("battery_store_mode_switch"), TEXT("{\"action\":3}")));

	// sdr_workmode_switch：linkWorkmode 仅 0/1
	TestEqual(TEXT("链路模式 linkWorkmode=0 成功"), Success, Bridge->HandleDebugCommand(TEXT("sdr_workmode_switch"), TEXT("{\"linkWorkmode\":0}")));
	TestEqual(TEXT("链路模式 linkWorkmode=2 越界失败"), InvalidParams, Bridge->HandleDebugCommand(TEXT("sdr_workmode_switch"), TEXT("{\"linkWorkmode\":2}")));
	TestEqual(TEXT("链路模式缺参失败"), InvalidParams, Bridge->HandleDebugCommand(TEXT("sdr_workmode_switch"), TEXT("")));

	// air_conditioner_mode_switch：action 0-3
	TestEqual(TEXT("空调 action=0 成功"), Success, Bridge->HandleDebugCommand(TEXT("air_conditioner_mode_switch"), TEXT("{\"action\":0}")));
	TestEqual(TEXT("空调 action=3 成功"), Success, Bridge->HandleDebugCommand(TEXT("air_conditioner_mode_switch"), TEXT("{\"action\":3}")));
	TestEqual(TEXT("空调 action=4 越界失败"), InvalidParams, Bridge->HandleDebugCommand(TEXT("air_conditioner_mode_switch"), TEXT("{\"action\":4}")));

	// 非法 JSON 与未知方法
	TestEqual(TEXT("非法 JSON 失败"), InvalidParams, Bridge->HandleDebugCommand(TEXT("battery_maintenance_switch"), TEXT("{broken")));
	TestEqual(TEXT("未知方法失败"), UnknownMethod, Bridge->HandleDebugCommand(TEXT("cover_toggle"), TEXT("")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVRemoteDebugProgressEventTest, "UAV.MqttBridge.RemoteDebug.ProgressEvent", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVRemoteDebugProgressEventTest::RunTest(const FString& Parameters)
{
	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	// sent 阶段：percent=0，stepKey 语义映射（cover_open → open_cover）
	const TSharedPtr<FJsonObject> Sent = Bridge->BuildRemoteDebugProgressEventData(TEXT("sent"), 0, 1, 1, TEXT("open_cover"), 0);
	TestTrue(TEXT("sent 事件 data 已组装"), Sent.IsValid());
	if (Sent.IsValid())
	{
		TestEqual(TEXT("result=0"), 0.0, Sent->GetNumberField(TEXT("result")));
		const TSharedPtr<FJsonObject> Output = Sent->GetObjectField(TEXT("output"));
		TestTrue(TEXT("output 对象存在"), Output.IsValid());
		if (Output.IsValid())
		{
			TestEqual(TEXT("output.status=sent"), TEXT("sent"), Output->GetStringField(TEXT("status")));
			const TSharedPtr<FJsonObject> Progress = Output->GetObjectField(TEXT("progress"));
			TestTrue(TEXT("progress 对象存在"), Progress.IsValid());
			if (Progress.IsValid())
			{
				TestEqual(TEXT("percent=0"), 0.0, Progress->GetNumberField(TEXT("percent")));
				TestEqual(TEXT("currentStep=1"), 1.0, Progress->GetNumberField(TEXT("currentStep")));
				TestEqual(TEXT("totalSteps=1"), 1.0, Progress->GetNumberField(TEXT("totalSteps")));
				TestEqual(TEXT("stepKey=open_cover"), TEXT("open_cover"), Progress->GetStringField(TEXT("stepKey")));
				TestEqual(TEXT("stepResult=0"), 0.0, Progress->GetNumberField(TEXT("stepResult")));
			}
		}
	}

	// ok 阶段：percent=100，stepKey 相同
	const TSharedPtr<FJsonObject> Ok = Bridge->BuildRemoteDebugProgressEventData(TEXT("ok"), 100, 1, 1, TEXT("open_cover"), 0);
	TestEqual(TEXT("ok percent=100"), 100.0, Ok->GetObjectField(TEXT("output"))->GetObjectField(TEXT("progress"))->GetNumberField(TEXT("percent")));
	TestEqual(TEXT("ok status=ok"), TEXT("ok"), Ok->GetObjectField(TEXT("output"))->GetStringField(TEXT("status")));

	// 无精确语义方法：stepKey 为空时省略字段（device_reboot / drone_close / drone_format / device_format / charge_open）
	const TSharedPtr<FJsonObject> NoStepKey = Bridge->BuildRemoteDebugProgressEventData(TEXT("sent"), 0, 1, 1, TEXT(""), 0);
	TestFalse(TEXT("缺省 stepKey 省略字段"), NoStepKey->GetObjectField(TEXT("output"))->GetObjectField(TEXT("progress"))->HasField(TEXT("stepKey")));

	// services_reply 成功回执带 output.status="sent"（对齐 ServicesReplyData<RemoteDebugResponse>）
	const TSharedRef<FJsonObject> Output = MakeShared<FJsonObject>();
	Output->SetStringField(TEXT("status"), TEXT("sent"));
	const TSharedRef<FJsonObject> Reply = UAV::CloudApi::MakeServicesReply(TEXT("cover_open"), TEXT("tid-1"), TEXT("bid-1"), 0, Output);
	const TSharedPtr<FJsonObject> ReplyData = Reply->GetObjectField(TEXT("data"));
	TestEqual(TEXT("回执 result=0"), 0.0, ReplyData->GetNumberField(TEXT("result")));
	TestEqual(TEXT("回执 output.status=sent"), TEXT("sent"), ReplyData->GetObjectField(TEXT("output"))->GetStringField(TEXT("status")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVRemoteDebugOsdLinkageTest, "UAV.MqttBridge.RemoteDebug.OsdLinkage", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVRemoteDebugOsdLinkageTest::RunTest(const FString& Parameters)
{
	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	UUAVDroneSimComponent* Sim = MakeDockedDroneSim();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	TestNotNull(TEXT("模拟组件可创建"), Sim);
	if (!Bridge || !Sim)
	{
		return false;
	}
	Bridge->SetDroneSim(Sim);

	// 场景序列：设备状态随调试指令联动
	Bridge->HandleDebugCommand(TEXT("debug_mode_open"), TEXT(""));
	Bridge->HandleDebugCommand(TEXT("cover_open"), TEXT(""));
	Bridge->HandleDebugCommand(TEXT("putter_open"), TEXT(""));
	Bridge->HandleDebugCommand(TEXT("supplement_light_open"), TEXT(""));
	Bridge->HandleDebugCommand(TEXT("alarm_state_switch"), TEXT("{\"action\":1}"));
	Bridge->HandleDebugCommand(TEXT("battery_store_mode_switch"), TEXT("{\"action\":2}"));
	Bridge->HandleDebugCommand(TEXT("air_conditioner_mode_switch"), TEXT("{\"action\":1}"));
	Bridge->HandleDebugCommand(TEXT("battery_maintenance_switch"), TEXT("{\"action\":1}"));
	Bridge->HandleDebugCommand(TEXT("sdr_workmode_switch"), TEXT("{\"linkWorkmode\":0}"));
	Bridge->HandleDebugCommand(TEXT("drone_close"), TEXT(""));

	const TSharedPtr<FJsonObject> Data = Bridge->BuildDockOsdPayload();
	TestTrue(TEXT("机场 OSD data 已组装"), Data.IsValid());
	if (!Data.IsValid())
	{
		return false;
	}

	// 调试模式：mode_code=2（REMOTE_DEBUGGING），覆盖 4/3 推导
	TestEqual(TEXT("调试模式 mode_code=2"), 2.0, Data->GetNumberField(TEXT("mode_code")));
	// 指令覆盖优先：待机在机场内仍按 cover_open 输出 1
	TestEqual(TEXT("cover_open 后 cover_state=1"), 1.0, Data->GetNumberField(TEXT("cover_state")));
	TestEqual(TEXT("putter_open 后 putter_state=1"), 1.0, Data->GetNumberField(TEXT("putter_state")));
	TestTrue(TEXT("supplement_light_open 后补光灯开启"), Data->GetBoolField(TEXT("supplement_light_state")));
	TestTrue(TEXT("alarm 开启后报警状态"), Data->GetBoolField(TEXT("alarm_state")));
	TestEqual(TEXT("电池存储模式=2"), 2.0, Data->GetNumberField(TEXT("battery_store_mode")));
	TestEqual(TEXT("空调模式=1"), 1.0, Data->GetObjectField(TEXT("air_conditioner"))->GetNumberField(TEXT("air_conditioner_state")));
	TestEqual(TEXT("电池保养开启 maintenance_state=1"), 1.0,
		Data->GetObjectField(TEXT("drone_battery_maintenance_info"))->GetNumberField(TEXT("maintenance_state")));
	TestEqual(TEXT("链路工作模式=0"), 0.0, Data->GetObjectField(TEXT("wireless_link"))->GetNumberField(TEXT("link_workmode")));
	TestEqual(TEXT("drone_close 后子设备离线"), 0.0, Data->GetObjectField(TEXT("sub_device"))->GetNumberField(TEXT("device_online_status")));

	// 充电覆盖：满电 + charge_open → state=1（指令优先于电量推导）
	Bridge->HandleDebugCommand(TEXT("charge_open"), TEXT(""));
	const TSharedPtr<FJsonObject> ChargeOpenData = Bridge->BuildDockOsdPayload();
	TestEqual(TEXT("charge_open 后满电充电状态=1"), 1.0,
		ChargeOpenData->GetObjectField(TEXT("drone_charge_state"))->GetNumberField(TEXT("state")));

	// charge_close → state=0（指令覆盖关闭）
	Bridge->HandleDebugCommand(TEXT("charge_close"), TEXT(""));
	const TSharedPtr<FJsonObject> ChargeCloseData = Bridge->BuildDockOsdPayload();
	TestEqual(TEXT("charge_close 后充电状态=0"), 0.0,
		ChargeCloseData->GetObjectField(TEXT("drone_charge_state"))->GetNumberField(TEXT("state")));

	// cover_close / debug_mode_close：状态复位，未覆盖字段回退推导
	Bridge->HandleDebugCommand(TEXT("cover_close"), TEXT(""));
	Bridge->HandleDebugCommand(TEXT("debug_mode_close"), TEXT(""));
	const TSharedPtr<FJsonObject> ResetData = Bridge->BuildDockOsdPayload();
	TestEqual(TEXT("cover_close 后 cover_state=0"), 0.0, ResetData->GetNumberField(TEXT("cover_state")));
	TestEqual(TEXT("关闭调试模式后 mode_code 回退 3（待机）"), 3.0, ResetData->GetNumberField(TEXT("mode_code")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVRemoteDebugFallbackTest, "UAV.MqttBridge.RemoteDebug.Fallback", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVRemoteDebugFallbackTest::RunTest(const FString& Parameters)
{
	// 未执行任何调试指令时：OSD 保持既有推导（待机在机场内 cover_state=0、满电 state=0）
	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	UUAVDroneSimComponent* Sim = MakeDockedDroneSim();
	if (!Bridge || !Sim)
	{
		return false;
	}
	Bridge->SetDroneSim(Sim);
	const TSharedPtr<FJsonObject> Data = Bridge->BuildDockOsdPayload();
	TestEqual(TEXT("默认 cover_state=0（归巢推导）"), 0.0, Data->GetNumberField(TEXT("cover_state")));
	TestEqual(TEXT("默认 mode_code=3（待机）"), 3.0, Data->GetNumberField(TEXT("mode_code")));
	TestEqual(TEXT("默认充电状态=0（满电）"), 0.0, Data->GetObjectField(TEXT("drone_charge_state"))->GetNumberField(TEXT("state")));
	TestEqual(TEXT("默认子设备在线"), 1.0, Data->GetObjectField(TEXT("sub_device"))->GetNumberField(TEXT("device_online_status")));
	TestFalse(TEXT("默认补光灯关闭"), Data->GetBoolField(TEXT("supplement_light_state")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
