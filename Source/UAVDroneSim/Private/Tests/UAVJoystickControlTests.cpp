// DRC 摇杆运动模型自动化测试：速度驱动/偏航/过期悬停/任务互斥
#include "Misc/AutomationTest.h"
#include "UAVDroneSimComponent.h"

#include "HAL/PlatformProcess.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVJoystickControlTest, "UAV.DroneSim.JoystickControl", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVJoystickControlTest::RunTest(const FString& Parameters)
{
	UUAVDroneSimComponent* Sim = NewObject<UUAVDroneSimComponent>();
	TestNotNull(TEXT("组件可创建"), Sim);
	if (!Sim)
	{
		return false;
	}

	// 摇杆驱动移动：机头朝北（朝向 0），右摇杆 y=5 → 沿机头右向（场景 +X/东）移动
	Sim->SetJoystickCommand(0, 5, 0, 0, 5, 500);
	TestTrue(TEXT("摇杆控制已激活"), Sim->IsJoystickActive());
	const double ExpectedSpeed = 5.0 * Sim->MaxHorizontalSpeed / 17.0;
	Sim->UpdateJoystickControl(1.0);
	TestEqual(TEXT("水平速度按归一化比例折算"), Sim->GetHorizontalSpeed(), ExpectedSpeed, 1e-3);
	TestEqual(TEXT("沿右向移动 X"), Sim->GetCurrentLocation().X, ExpectedSpeed, 1e-3);
	TestEqual(TEXT("Y 方向无位移"), Sim->GetCurrentLocation().Y, 0.0, 1e-6);
	TestEqual(TEXT("朝向保持北向"), Sim->GetHeadingDegrees(), 0.0, 1e-6);

	// 前向摇杆 x=17 → 沿机头前向（场景 +Y/北）满速
	Sim->SetJoystickCommand(17, 0, 0, 0, 5, 500);
	Sim->UpdateJoystickControl(0.5);
	TestEqual(TEXT("x=17 满速前向"), Sim->GetHorizontalSpeed(), Sim->MaxHorizontalSpeed, 1e-3);
	TestEqual(TEXT("前向移动 Y"), Sim->GetCurrentLocation().Y, Sim->MaxHorizontalSpeed * 0.5, 1e-3);

	// 偏航控制：w=30 度/秒持续右转，位置不水平移动
	UUAVDroneSimComponent* Yaw = NewObject<UUAVDroneSimComponent>();
	Yaw->SetJoystickCommand(0, 0, 0, 30, 5, 500);
	Yaw->UpdateJoystickControl(1.0);
	TestEqual(TEXT("偏航 30 度"), Yaw->GetHeadingDegrees(), 30.0, 1e-6);
	TestEqual(TEXT("纯偏航无水平位移"), Yaw->GetCurrentLocation().Size(), 0.0, 1e-6);

	// 悬停（全 0 指令）：位置保持
	UUAVDroneSimComponent* Hover = NewObject<UUAVDroneSimComponent>();
	Hover->SetJoystickCommand(0, 0, 0, 0, 5, 500);
	Hover->UpdateJoystickControl(1.0);
	TestEqual(TEXT("全 0 指令悬停"), Hover->GetCurrentLocation().Size(), 0.0, 1e-6);
	TestEqual(TEXT("悬停速度为零"), Hover->GetHorizontalSpeed(), 0.0, 1e-6);

	// 指令过期悬停：delayTime 100ms，超过后目标归零、速度衰减收敛
	UUAVDroneSimComponent* Expire = NewObject<UUAVDroneSimComponent>();
	Expire->SetJoystickCommand(0, 5, 0, 0, 5, 100);
	Expire->UpdateJoystickControl(0.1);
	TestTrue(TEXT("有效期内建立速度"), Expire->GetHorizontalSpeed() > 1.0);
	FPlatformProcess::Sleep(0.3f); // 超过 100ms 有效期
	Expire->UpdateJoystickControl(0.1);
	const double SpeedAfterExpiry1 = Expire->GetHorizontalSpeed();
	Expire->UpdateJoystickControl(0.1);
	const double SpeedAfterExpiry2 = Expire->GetHorizontalSpeed();
	TestTrue(TEXT("过期后速度递减"), SpeedAfterExpiry2 < SpeedAfterExpiry1);
	for (int32 Index = 0; Index < 20; ++Index)
	{
		Expire->UpdateJoystickControl(0.1);
	}
	TestTrue(TEXT("过期后收敛至悬停"), Expire->GetHorizontalSpeed() < 0.01);
	TestTrue(TEXT("过期后会话保持"), Expire->IsJoystickActive());

	// 摇杆模式与任务互斥：进入摇杆控制时现有任务被停止
	UUAVDroneSimComponent* Mission = NewObject<UUAVDroneSimComponent>();
	FUAVWaypoint Waypoint;
	Waypoint.Latitude = 30.123456;
	Waypoint.Longitude = 120.654321;
	Waypoint.Altitude = 100.0;
	TArray<FUAVWaypoint> Waypoints;
	Waypoints.Add(Waypoint);
	TestTrue(TEXT("任务可启动"), Mission->SetWaypoints(Waypoints, true));
	TestTrue(TEXT("任务进行中"), Mission->HasActiveMission());
	Mission->SetJoystickCommand(0, 5, 0, 0, 5, 500);
	TestFalse(TEXT("进入摇杆控制后任务停止"), Mission->HasActiveMission());
	TestTrue(TEXT("摇杆控制激活"), Mission->IsJoystickActive());
	Mission->SetJoystickActive(false);
	TestFalse(TEXT("停用摇杆控制"), Mission->IsJoystickActive());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
