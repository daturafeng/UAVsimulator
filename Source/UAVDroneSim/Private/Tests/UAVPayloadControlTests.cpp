// 载荷控制接口自动化测试：拍照状态/录像覆盖/云台指令目标/载荷权（任务 1.5）
#include "Misc/AutomationTest.h"
#include "UAVDroneSimComponent.h"
#include "UAVPayloadMath.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVPhotoStateTest, "UAV.Payload.PhotoState", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVPhotoStateTest::RunTest(const FString& Parameters)
{
	UUAVDroneSimComponent* Sim = NewObject<UUAVDroneSimComponent>();
	TestNotNull(TEXT("组件可创建"), Sim);
	if (!Sim)
	{
		return false;
	}

	// 初始状态：未拍照、剩余 9999、累计 0
	TestFalse(TEXT("初始未拍照"), Sim->IsPhotoTaking());
	TestEqual(TEXT("初始剩余照片数"), Sim->GetRemainingPhotoNum(), 9999);
	TestEqual(TEXT("初始累计照片数"), Sim->GetTakenPhotoCount(), 0);

	// 拍照推进：剩余减 1、累计加 1、进入拍照中
	Sim->TakePhoto();
	TestTrue(TEXT("拍照中状态"), Sim->IsPhotoTaking());
	TestEqual(TEXT("拍照后剩余"), Sim->GetRemainingPhotoNum(), 9998);
	TestEqual(TEXT("拍照后累计"), Sim->GetTakenPhotoCount(), 1);

	// 指令结束拍照
	Sim->SetPhotoTaking(false);
	TestFalse(TEXT("指令结束拍照"), Sim->IsPhotoTaking());

	// 剩余照片数下限：耗尽后保持 0，累计仍加 1
	UUAVDroneSimComponent* Exhausted = NewObject<UUAVDroneSimComponent>();
	for (int32 Index = 0; Index < 9999; ++Index)
	{
		Exhausted->TakePhoto();
	}
	TestEqual(TEXT("耗尽剩余照片数"), Exhausted->GetRemainingPhotoNum(), 0);
	TestEqual(TEXT("耗尽累计照片数"), Exhausted->GetTakenPhotoCount(), 9999);
	Exhausted->TakePhoto();
	TestEqual(TEXT("耗尽后再拍剩余保持 0"), Exhausted->GetRemainingPhotoNum(), 0);
	TestEqual(TEXT("耗尽后再拍累计仍加 1"), Exhausted->GetTakenPhotoCount(), 10000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVRecordingOverrideTest, "UAV.Payload.RecordingOverride", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVRecordingOverrideTest::RunTest(const FString& Parameters)
{
	UUAVDroneSimComponent* Sim = NewObject<UUAVDroneSimComponent>();
	TestNotNull(TEXT("组件可创建"), Sim);
	if (!Sim)
	{
		return false;
	}

	// 待机默认不录像
	Sim->SetFlightState(EUAVFlightState::Idle);
	TestFalse(TEXT("待机默认不录像"), Sim->IsRecording());

	// 指令覆盖优先：待机 StartRecording 后录像中
	Sim->StartRecording();
	TestTrue(TEXT("待机指令开始录像"), Sim->IsRecording());

	// 停止覆盖：航线状态 StopRecording 后不录像
	Sim->SetFlightState(EUAVFlightState::Wayline);
	Sim->StopRecording();
	TestFalse(TEXT("航线指令停止录像"), Sim->IsRecording());

	// 清除覆盖恢复推导：航线恢复录像
	Sim->ClearRecordingOverride();
	TestTrue(TEXT("航线推导录像中"), Sim->IsRecording());

	// 清除覆盖后待机不录像
	Sim->SetFlightState(EUAVFlightState::Idle);
	TestFalse(TEXT("待机推导不录像"), Sim->IsRecording());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVGimbalTargetTest, "UAV.Payload.GimbalTarget", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVGimbalTargetTest::RunTest(const FString& Parameters)
{
	// 组件目标存储与复位
	UUAVDroneSimComponent* Sim = NewObject<UUAVDroneSimComponent>();
	TestNotNull(TEXT("组件可创建"), Sim);
	if (!Sim)
	{
		return false;
	}
	TestFalse(TEXT("初始无云台目标"), Sim->HasGimbalTarget());
	Sim->SetGimbalTarget(-30.0, 45.0);
	TestTrue(TEXT("设置后存在云台目标"), Sim->HasGimbalTarget());
	TestEqual(TEXT("目标俯仰"), Sim->GetGimbalTargetPitch(), -30.0, 1e-6);
	TestEqual(TEXT("目标偏航"), Sim->GetGimbalTargetYaw(), 45.0, 1e-6);
	Sim->ResetGimbalTarget();
	TestFalse(TEXT("复位后无云台目标"), Sim->HasGimbalTarget());

	// 纯函数叠加：目标覆盖俯仰/偏航，横滚保持
	const FUAVGimbalState Base = UAVPayloadMath::ComputeGimbalState(90.0, 0.0, FUAVGimbalConfig());
	const FUAVGimbalState Applied = UAVPayloadMath::ApplyGimbalTarget(Base, -30.0, 45.0);
	TestEqual(TEXT("叠加俯仰等于目标"), Applied.PitchDegrees, -30.0, 1e-6);
	TestEqual(TEXT("叠加偏航等于目标"), Applied.YawDegrees, 45.0, 1e-6);
	TestEqual(TEXT("横滚保持微动"), Applied.RollDegrees, Base.RollDegrees, 1e-6);

	// 偏航归一化到 0-360
	const FUAVGimbalState Normalized = UAVPayloadMath::ApplyGimbalTarget(Base, 0.0, 400.0);
	TestEqual(TEXT("偏航超周归一化"), Normalized.YawDegrees, 40.0, 1e-6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVPayloadAuthorityTest, "UAV.Payload.PayloadAuthority", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVPayloadAuthorityTest::RunTest(const FString& Parameters)
{
	UUAVDroneSimComponent* Sim = NewObject<UUAVDroneSimComponent>();
	TestNotNull(TEXT("组件可创建"), Sim);
	if (!Sim)
	{
		return false;
	}
	TestFalse(TEXT("初始未抢占载荷权"), Sim->HasPayloadAuthority());
	Sim->SetPayloadAuthority(true);
	TestTrue(TEXT("抢占后持有载荷权"), Sim->HasPayloadAuthority());
	Sim->SetPayloadAuthority(false);
	TestFalse(TEXT("释放后无载荷权"), Sim->HasPayloadAuthority());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
