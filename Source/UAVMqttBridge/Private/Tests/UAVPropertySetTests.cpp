// 物模型属性设置自动化测试：属性校验 + 回执结构 + OSD 联动（任务 4.x）
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVPropertySetValidSettingsTest, "UAV.MqttBridge.PropertySet.ValidSettings", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVPropertySetValidSettingsTest::RunTest(const FString& Parameters)
{
	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	// 7 个无人机属性 + 1 个机场属性合法设置各一次
	TestEqual(TEXT("night_lights_state=1 成功"), 0, Bridge->HandlePropertySet(TEXT("{\"night_lights_state\":{\"night_lights_state\":1}}")));
	TestEqual(TEXT("height_limit=120 成功"), 0, Bridge->HandlePropertySet(TEXT("{\"height_limit\":{\"height_limit\":120}}")));
	TestEqual(TEXT("distance_limit_status 双字段成功"), 0, Bridge->HandlePropertySet(TEXT("{\"distance_limit_status\":{\"state\":1,\"distance_limit\":3000}}")));
	TestEqual(TEXT("distance_limit_status 仅 state 成功"), 0, Bridge->HandlePropertySet(TEXT("{\"distance_limit_status\":{\"state\":0}}")));
	TestEqual(TEXT("distance_limit_status 仅 distance_limit 成功"), 0, Bridge->HandlePropertySet(TEXT("{\"distance_limit_status\":{\"distance_limit\":1500}}")));
	TestEqual(TEXT("obstacle_avoidance 三项成功"), 0, Bridge->HandlePropertySet(TEXT("{\"obstacle_avoidance\":{\"horizon\":0,\"upside\":1,\"downside\":1}}")));
	TestEqual(TEXT("obstacle_avoidance 仅一项成功"), 0, Bridge->HandlePropertySet(TEXT("{\"obstacle_avoidance\":{\"horizon\":1}}")));
	TestEqual(TEXT("rth_altitude=100 成功"), 0, Bridge->HandlePropertySet(TEXT("{\"rth_altitude\":{\"rth_altitude\":100}}")));
	TestEqual(TEXT("rc_lost_action=0 成功"), 0, Bridge->HandlePropertySet(TEXT("{\"rc_lost_action\":{\"rc_lost_action\":0}}")));
	TestEqual(TEXT("exit_wayline_when_rc_lost=0 成功"), 0, Bridge->HandlePropertySet(TEXT("{\"exit_wayline_when_rc_lost\":{\"exit_wayline_when_rc_lost\":0}}")));
	TestEqual(TEXT("user_experience_improvement=1 成功"), 0, Bridge->HandlePropertySet(TEXT("{\"user_experience_improvement\":{\"user_experience_improvement\":1}}")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVPropertySetInvalidSettingsTest, "UAV.MqttBridge.PropertySet.InvalidSettings", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVPropertySetInvalidSettingsTest::RunTest(const FString& Parameters)
{
	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	// 越界 / 缺字段 / 未知属性 / 非法 JSON 一律回 result=1
	TestEqual(TEXT("night_lights_state=2 越界失败"), 1, Bridge->HandlePropertySet(TEXT("{\"night_lights_state\":{\"night_lights_state\":2}}")));
	TestEqual(TEXT("night_lights_state 缺字段失败"), 1, Bridge->HandlePropertySet(TEXT("{\"night_lights_state\":{}}")));
	TestEqual(TEXT("height_limit=10 过小失败"), 1, Bridge->HandlePropertySet(TEXT("{\"height_limit\":{\"height_limit\":10}}")));
	TestEqual(TEXT("height_limit=1501 过大失败"), 1, Bridge->HandlePropertySet(TEXT("{\"height_limit\":{\"height_limit\":1501}}")));
	TestEqual(TEXT("rth_altitude=19 过小失败"), 1, Bridge->HandlePropertySet(TEXT("{\"rth_altitude\":{\"rth_altitude\":19}}")));
	TestEqual(TEXT("rth_altitude=501 过大失败"), 1, Bridge->HandlePropertySet(TEXT("{\"rth_altitude\":{\"rth_altitude\":501}}")));
	TestEqual(TEXT("distance_limit_status 空失败"), 1, Bridge->HandlePropertySet(TEXT("{\"distance_limit_status\":{}}")));
	TestEqual(TEXT("distance_limit_status state=2 失败"), 1, Bridge->HandlePropertySet(TEXT("{\"distance_limit_status\":{\"state\":2}}")));
	TestEqual(TEXT("distance_limit_status distance=14 失败"), 1, Bridge->HandlePropertySet(TEXT("{\"distance_limit_status\":{\"distance_limit\":14}}")));
	TestEqual(TEXT("distance_limit_status distance=8001 失败"), 1, Bridge->HandlePropertySet(TEXT("{\"distance_limit_status\":{\"distance_limit\":8001}}")));
	TestEqual(TEXT("obstacle_avoidance 空失败"), 1, Bridge->HandlePropertySet(TEXT("{\"obstacle_avoidance\":{}}")));
	TestEqual(TEXT("obstacle_avoidance horizon=2 失败"), 1, Bridge->HandlePropertySet(TEXT("{\"obstacle_avoidance\":{\"horizon\":2}}")));
	TestEqual(TEXT("rc_lost_action=3 越界失败"), 1, Bridge->HandlePropertySet(TEXT("{\"rc_lost_action\":{\"rc_lost_action\":3}}")));
	TestEqual(TEXT("exit_wayline_when_rc_lost=2 越界失败"), 1, Bridge->HandlePropertySet(TEXT("{\"exit_wayline_when_rc_lost\":{\"exit_wayline_when_rc_lost\":2}}")));
	TestEqual(TEXT("user_experience_improvement=9 越界失败"), 1, Bridge->HandlePropertySet(TEXT("{\"user_experience_improvement\":{\"user_experience_improvement\":9}}")));
	TestEqual(TEXT("未知属性 silent_mode 失败"), 1, Bridge->HandlePropertySet(TEXT("{\"silent_mode\":{\"silent_mode\":1}}")));
	TestEqual(TEXT("缺 data 空串失败"), 1, Bridge->HandlePropertySet(TEXT("")));
	TestEqual(TEXT("非法 JSON 失败"), 1, Bridge->HandlePropertySet(TEXT("{broken")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVPropertySetOsdLinkageTest, "UAV.MqttBridge.PropertySet.OsdLinkage", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVPropertySetOsdLinkageTest::RunTest(const FString& Parameters)
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

	// 默认属性值输出（未收到任何属性设置）
	const TSharedPtr<FJsonObject> DefaultDrone = Bridge->BuildDroneOsdPayload();
	TestEqual(TEXT("默认 night_lights_state=0"), 0.0, DefaultDrone->GetNumberField(TEXT("night_lights_state")));
	TestEqual(TEXT("默认 height_limit=500"), 500.0, DefaultDrone->GetNumberField(TEXT("height_limit")));
	const TSharedPtr<FJsonObject> DefaultDistance = DefaultDrone->GetObjectField(TEXT("distance_limit_status"));
	TestEqual(TEXT("默认 distance_limit_status.state=1"), 1.0, DefaultDistance->GetNumberField(TEXT("state")));
	TestEqual(TEXT("默认 distance_limit_status.distance_limit=3000"), 3000.0, DefaultDistance->GetNumberField(TEXT("distance_limit")));
	TestFalse(TEXT("默认 distance_limit_status.is_near_distance_limit=false"), DefaultDistance->GetBoolField(TEXT("is_near_distance_limit")));
	const TSharedPtr<FJsonObject> DefaultObstacle = DefaultDrone->GetObjectField(TEXT("obstacle_avoidance"));
	TestEqual(TEXT("默认避障 horizon=1"), 1.0, DefaultObstacle->GetNumberField(TEXT("horizon")));
	TestEqual(TEXT("默认避障 upside=1"), 1.0, DefaultObstacle->GetNumberField(TEXT("upside")));
	TestEqual(TEXT("默认避障 downside=1"), 1.0, DefaultObstacle->GetNumberField(TEXT("downside")));
	TestEqual(TEXT("默认 rc_lost_action=2"), 2.0, DefaultDrone->GetNumberField(TEXT("rc_lost_action")));
	TestEqual(TEXT("默认 rth_altitude=60"), 60.0, DefaultDrone->GetNumberField(TEXT("rth_altitude")));
	TestEqual(TEXT("默认 exit_wayline_when_rc_lost=1"), 1.0, DefaultDrone->GetNumberField(TEXT("exit_wayline_when_rc_lost")));
	const TSharedPtr<FJsonObject> DefaultDock = Bridge->BuildDockOsdPayload();
	TestEqual(TEXT("默认 user_experience_improvement=2"), 2.0, DefaultDock->GetNumberField(TEXT("user_experience_improvement")));

	// 设置一批属性后重新组装 OSD，断言联动
	TestEqual(TEXT("设置 height_limit=120"), 0, Bridge->HandlePropertySet(TEXT("{\"height_limit\":{\"height_limit\":120}}")));
	TestEqual(TEXT("设置 rth_altitude=100"), 0, Bridge->HandlePropertySet(TEXT("{\"rth_altitude\":{\"rth_altitude\":100}}")));
	TestEqual(TEXT("设置 night_lights_state=1"), 0, Bridge->HandlePropertySet(TEXT("{\"night_lights_state\":{\"night_lights_state\":1}}")));
	TestEqual(TEXT("设置 distance_limit_status"), 0, Bridge->HandlePropertySet(TEXT("{\"distance_limit_status\":{\"state\":0,\"distance_limit\":1500}}")));
	TestEqual(TEXT("设置 obstacle_avoidance"), 0, Bridge->HandlePropertySet(TEXT("{\"obstacle_avoidance\":{\"horizon\":0,\"upside\":1,\"downside\":0}}")));
	TestEqual(TEXT("设置 rc_lost_action=0"), 0, Bridge->HandlePropertySet(TEXT("{\"rc_lost_action\":{\"rc_lost_action\":0}}")));
	TestEqual(TEXT("设置 exit_wayline_when_rc_lost=0"), 0, Bridge->HandlePropertySet(TEXT("{\"exit_wayline_when_rc_lost\":{\"exit_wayline_when_rc_lost\":0}}")));
	TestEqual(TEXT("设置 user_experience_improvement=1"), 0, Bridge->HandlePropertySet(TEXT("{\"user_experience_improvement\":{\"user_experience_improvement\":1}}")));

	const TSharedPtr<FJsonObject> Drone = Bridge->BuildDroneOsdPayload();
	TestEqual(TEXT("联动 night_lights_state=1"), 1.0, Drone->GetNumberField(TEXT("night_lights_state")));
	TestEqual(TEXT("联动 height_limit=120"), 120.0, Drone->GetNumberField(TEXT("height_limit")));
	const TSharedPtr<FJsonObject> Distance = Drone->GetObjectField(TEXT("distance_limit_status"));
	TestEqual(TEXT("联动 distance_limit_status.state=0"), 0.0, Distance->GetNumberField(TEXT("state")));
	TestEqual(TEXT("联动 distance_limit_status.distance_limit=1500"), 1500.0, Distance->GetNumberField(TEXT("distance_limit")));
	const TSharedPtr<FJsonObject> Obstacle = Drone->GetObjectField(TEXT("obstacle_avoidance"));
	TestEqual(TEXT("联动避障 horizon=0"), 0.0, Obstacle->GetNumberField(TEXT("horizon")));
	TestEqual(TEXT("联动避障 upside=1"), 1.0, Obstacle->GetNumberField(TEXT("upside")));
	TestEqual(TEXT("联动避障 downside=0"), 0.0, Obstacle->GetNumberField(TEXT("downside")));
	TestEqual(TEXT("联动 rc_lost_action=0"), 0.0, Drone->GetNumberField(TEXT("rc_lost_action")));
	TestEqual(TEXT("联动 rth_altitude=100"), 100.0, Drone->GetNumberField(TEXT("rth_altitude")));
	TestEqual(TEXT("联动 exit_wayline_when_rc_lost=0"), 0.0, Drone->GetNumberField(TEXT("exit_wayline_when_rc_lost")));
	const TSharedPtr<FJsonObject> Dock = Bridge->BuildDockOsdPayload();
	TestEqual(TEXT("联动 user_experience_improvement=1"), 1.0, Dock->GetNumberField(TEXT("user_experience_improvement")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVPropertySetDispatchReplyTest, "UAV.MqttBridge.PropertySet.DispatchReply", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVPropertySetDispatchReplyTest::RunTest(const FString& Parameters)
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

	// 未连接时分发合法报文：Publish 内部判空安全，属性状态仍更新
	Bridge->DispatchPropertySetMessage(TEXT("{\"tid\":\"tid-1\",\"bid\":\"bid-1\",\"timestamp\":1724745600000,\"data\":{\"height_limit\":{\"height_limit\":120}}}"));
	TestEqual(TEXT("分发后 height_limit=120"), 120.0, Bridge->BuildDroneOsdPayload()->GetNumberField(TEXT("height_limit")));

	// 缺 data 分发：状态不变（仍为 120）
	Bridge->DispatchPropertySetMessage(TEXT("{\"tid\":\"tid-2\",\"bid\":\"bid-2\",\"timestamp\":1724745600000}"));
	TestEqual(TEXT("缺 data 分发状态不变"), 120.0, Bridge->BuildDroneOsdPayload()->GetNumberField(TEXT("height_limit")));

	// 回执结构：tid/bid 回填、无 method、含 timestamp、data.result
	const TSharedPtr<FJsonObject> SuccessReply = Bridge->BuildPropertySetReply(TEXT("tid-abc"), TEXT("bid-xyz"), 0);
	TestEqual(TEXT("回执 tid 回填"), TEXT("tid-abc"), SuccessReply->GetStringField(TEXT("tid")));
	TestEqual(TEXT("回执 bid 回填"), TEXT("bid-xyz"), SuccessReply->GetStringField(TEXT("bid")));
	TestTrue(TEXT("回执含 timestamp"), SuccessReply->HasField(TEXT("timestamp")));
	TestFalse(TEXT("回执无 method"), SuccessReply->HasField(TEXT("method")));
	TestEqual(TEXT("成功回执 result=0"), 0.0, SuccessReply->GetObjectField(TEXT("data"))->GetNumberField(TEXT("result")));

	const TSharedPtr<FJsonObject> FailedReply = Bridge->BuildPropertySetReply(TEXT("tid-abc"), TEXT("bid-xyz"), 1);
	TestEqual(TEXT("失败回执 result=1"), 1.0, FailedReply->GetObjectField(TEXT("data"))->GetNumberField(TEXT("result")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
