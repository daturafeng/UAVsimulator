// UAVPayloadMath 自动化测试：电量/电池/云台/风向推导（任务 1.2）
#include "Misc/AutomationTest.h"
#include "UAVPayloadMath.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVBatteryDrainTest, "UAV.Payload.BatteryDrain", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVBatteryDrainTest::RunTest(const FString& Parameters)
{
	// 飞行消耗：100% 电量，0.05%/秒，10 秒后 99.5%
	TestEqual(TEXT("飞行 10 秒消耗 0.5%"), UAVPayloadMath::DrainBatteryPercent(100.0, true, 10.0, 0.05, 0.005), 99.5, 1e-6);
	// 待机消耗：0.005%/秒
	TestEqual(TEXT("待机 10 秒消耗 0.05%"), UAVPayloadMath::DrainBatteryPercent(100.0, false, 10.0, 0.05, 0.005), 99.95, 1e-6);
	// 到零钳制
	TestEqual(TEXT("电量不低于 0"), UAVPayloadMath::DrainBatteryPercent(1.0, true, 60.0, 0.05, 0.005), 0.0, 1e-6);
	// 负 DeltaTime 不消耗
	TestEqual(TEXT("负时间不消耗"), UAVPayloadMath::DrainBatteryPercent(80.0, true, -1.0, 0.05, 0.005), 80.0, 1e-6);
	// 剩余飞行时间：100% / 0.05% 每秒 = 2000 秒
	TestEqual(TEXT("剩余飞行时间"), UAVPayloadMath::EstimateRemainFlightTimeSeconds(100.0, 0.05), 2000.0, 1e-6);
	TestEqual(TEXT("速率 0 时剩余时间为 0"), UAVPayloadMath::EstimateRemainFlightTimeSeconds(100.0, 0.0), 0.0, 1e-6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVBatteryCellTest, "UAV.Payload.BatteryCell", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVBatteryCellTest::RunTest(const FString& Parameters)
{
	// 温度：27.5 + (100-100)*0.06 + 0*0.18 = 27.5
	TestEqual(TEXT("满电静止温度"), UAVPayloadMath::ComputeBatteryTemperatureCelsius(100.0, 0.0), 27.5, 1e-6);
	// 温度：27.5 + (100-50)*0.06 + 10*0.18 = 27.5+3.0+1.8 = 32.3
	TestEqual(TEXT("50% 电量 10m/s 温度"), UAVPayloadMath::ComputeBatteryTemperatureCelsius(50.0, 10.0), 32.3, 1e-6);
	// 电压：25800 + 100*18 - 0*35 = 27600
	TestEqual(TEXT("满电静止电压"), UAVPayloadMath::ComputeBatteryVoltageMv(100.0, 0.0), 27600);
	// 电压：25800 + 50*18 - 10*35 = 25800+900-350 = 26350
	TestEqual(TEXT("50% 电量 10m/s 电压"), UAVPayloadMath::ComputeBatteryVoltageMv(50.0, 10.0), 26350);
	// 电压下限 22000：0% 电量 200m/s → 25800-7000 = 18800 → 22000
	TestEqual(TEXT("电压下限"), UAVPayloadMath::ComputeBatteryVoltageMv(0.0, 200.0), 22000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVGimbalStateTest, "UAV.Payload.GimbalState", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVGimbalStateTest::RunTest(const FString& Parameters)
{
	FUAVGimbalConfig Config;
	Config.PitchBaseDegrees = -8.0;
	Config.PitchAmplitudeDegrees = 6.0;
	Config.RollAmplitudeDegrees = 1.2;
	Config.YawSwayAmplitudeDegrees = 4.0;
	Config.bYawFollowsHeading = true;

	// 时间 0：pitch=-8、roll=1.2、yaw=朝向（微动 0）
	const FUAVGimbalState T0 = UAVPayloadMath::ComputeGimbalState(90.0, 0.0, Config);
	TestEqual(TEXT("时间 0 俯仰基线"), T0.PitchDegrees, -8.0, 1e-6);
	TestEqual(TEXT("时间 0 横滚"), T0.RollDegrees, 1.2, 1e-6);
	TestEqual(TEXT("时间 0 偏航跟随"), T0.YawDegrees, 90.0, 1e-6);

	// 时间推进后俯仰在基线 ± 振幅范围内，偏航归一化到 [0,360)
	const FUAVGimbalState T1 = UAVPayloadMath::ComputeGimbalState(350.0, 10.0, Config);
	TestTrue(TEXT("俯仰在振幅范围内"), T1.PitchDegrees >= -14.0 && T1.PitchDegrees <= -2.0);
	TestTrue(TEXT("偏航归一化"), T1.YawDegrees >= 0.0 && T1.YawDegrees < 360.0);
	TestTrue(TEXT("偏航靠近朝向 ± 振幅"), FMath::Abs(UAVPayloadMath::NormalizeDegrees(T1.YawDegrees - 350.0)) <= 4.1);

	// 不跟随：偏航等于朝向（无微动）
	Config.bYawFollowsHeading = false;
	const FUAVGimbalState T2 = UAVPayloadMath::ComputeGimbalState(-10.0, 30.0, Config);
	TestEqual(TEXT("不跟随偏航归一化"), T2.YawDegrees, 350.0, 1e-6);

	// 归一化：负角与大于 360 的角度
	TestEqual(TEXT("负角归一化"), UAVPayloadMath::NormalizeDegrees(-10.0), 350.0, 1e-6);
	TestEqual(TEXT("超周归一化"), UAVPayloadMath::NormalizeDegrees(370.0), 10.0, 1e-6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVWindDirectionTest, "UAV.Payload.WindDirection", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVWindDirectionTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("北 0"), UAVPayloadMath::ComputeWindDirectionEnum(0.0), 0);
	TestEqual(TEXT("东北 1"), UAVPayloadMath::ComputeWindDirectionEnum(45.0), 1);
	TestEqual(TEXT("东 2"), UAVPayloadMath::ComputeWindDirectionEnum(90.0), 2);
	TestEqual(TEXT("东南 3"), UAVPayloadMath::ComputeWindDirectionEnum(135.0), 3);
	TestEqual(TEXT("南 4"), UAVPayloadMath::ComputeWindDirectionEnum(180.0), 4);
	TestEqual(TEXT("西南 5"), UAVPayloadMath::ComputeWindDirectionEnum(225.0), 5);
	TestEqual(TEXT("西 6"), UAVPayloadMath::ComputeWindDirectionEnum(270.0), 6);
	TestEqual(TEXT("西北 7"), UAVPayloadMath::ComputeWindDirectionEnum(315.0), 7);
	TestEqual(TEXT("北边界下沿"), UAVPayloadMath::ComputeWindDirectionEnum(22.4), 0);
	TestEqual(TEXT("北边界上沿"), UAVPayloadMath::ComputeWindDirectionEnum(337.5), 0);
	TestEqual(TEXT("负角"), UAVPayloadMath::ComputeWindDirectionEnum(-90.0), 6);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
