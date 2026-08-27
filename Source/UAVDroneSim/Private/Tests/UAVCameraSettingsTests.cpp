// 相机设置状态自动化测试：曝光/对焦/测光/存储/分屏/焦距/看点/POI（任务 1.5）
#include "Misc/AutomationTest.h"
#include "UAVDroneSimComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVCameraExposureTest, "UAV.Payload.CameraSettings.Exposure", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVCameraExposureTest::RunTest(const FString& Parameters)
{
	UUAVDroneSimComponent* Sim = NewObject<UUAVDroneSimComponent>();
	TestNotNull(TEXT("组件可创建"), Sim);
	if (!Sim)
	{
		return false;
	}

	// 默认值：自动曝光、1/1000 快门、ISO 100、补偿 0
	TestEqual(TEXT("默认曝光模式"), Sim->GetExposureMode(), 0);
	TestEqual(TEXT("默认快门速度"), Sim->GetShutterSpeed(), 1.0 / 1000.0, 1e-9);
	TestEqual(TEXT("默认 ISO"), Sim->GetIso(), 100);
	TestEqual(TEXT("默认曝光补偿"), Sim->GetExposureCompensation(), 0.0, 1e-9);

	// 正常设置
	Sim->SetExposureMode(2);
	Sim->SetShutterSpeed(1.0 / 60.0);
	Sim->SetIso(800);
	Sim->SetExposureCompensation(-1.5);
	TestEqual(TEXT("设置快门优先"), Sim->GetExposureMode(), 2);
	TestEqual(TEXT("设置快门速度"), Sim->GetShutterSpeed(), 1.0 / 60.0, 1e-9);
	TestEqual(TEXT("设置 ISO"), Sim->GetIso(), 800);
	TestEqual(TEXT("设置曝光补偿"), Sim->GetExposureCompensation(), -1.5, 1e-9);

	// 钳制：模式 0-3、快门 1/8000-1 秒、ISO 50-12800、补偿 ±3
	Sim->SetExposureMode(9);
	TestEqual(TEXT("曝光模式上限钳制"), Sim->GetExposureMode(), 3);
	Sim->SetExposureMode(-1);
	TestEqual(TEXT("曝光模式下限钳制"), Sim->GetExposureMode(), 0);
	Sim->SetShutterSpeed(2.0);
	TestEqual(TEXT("快门上限钳制"), Sim->GetShutterSpeed(), 1.0, 1e-9);
	Sim->SetShutterSpeed(0.0);
	TestEqual(TEXT("快门下限钳制"), Sim->GetShutterSpeed(), 1.0 / 8000.0, 1e-9);
	Sim->SetIso(99999);
	TestEqual(TEXT("ISO 上限钳制"), Sim->GetIso(), 12800);
	Sim->SetIso(1);
	TestEqual(TEXT("ISO 下限钳制"), Sim->GetIso(), 50);
	Sim->SetExposureCompensation(5.0);
	TestEqual(TEXT("补偿上限钳制"), Sim->GetExposureCompensation(), 3.0, 1e-9);
	Sim->SetExposureCompensation(-5.0);
	TestEqual(TEXT("补偿下限钳制"), Sim->GetExposureCompensation(), -3.0, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVCameraFocusMeteringTest, "UAV.Payload.CameraSettings.FocusMetering", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVCameraFocusMeteringTest::RunTest(const FString& Parameters)
{
	UUAVDroneSimComponent* Sim = NewObject<UUAVDroneSimComponent>();
	TestNotNull(TEXT("组件可创建"), Sim);
	if (!Sim)
	{
		return false;
	}

	// 对焦：模式 0-2、对焦值 0-100、点对焦动作记录
	Sim->SetFocusMode(1);
	TestEqual(TEXT("设置单次自动对焦"), Sim->GetFocusMode(), 1);
	Sim->SetFocusMode(9);
	TestEqual(TEXT("对焦模式上限钳制"), Sim->GetFocusMode(), 2);
	Sim->SetFocusValue(42);
	TestEqual(TEXT("设置对焦值"), Sim->GetFocusValue(), 42);
	Sim->SetFocusValue(150);
	TestEqual(TEXT("对焦值上限钳制"), Sim->GetFocusValue(), 100);
	Sim->SetFocusValue(-3);
	TestEqual(TEXT("对焦值下限钳制"), Sim->GetFocusValue(), 0);
	Sim->SetPointFocusAction(TEXT("point_focus_start"));
	TestEqual(TEXT("记录点对焦开始"), Sim->GetPointFocusAction(), TEXT("point_focus_start"));

	// 红外测光：模式 0-2、点坐标 0-1、区域宽高下限 0.01
	Sim->SetIrMeteringMode(2);
	TestEqual(TEXT("设置区域测光"), Sim->GetIrMeteringMode(), 2);
	Sim->SetIrMeteringMode(5);
	TestEqual(TEXT("测光模式上限钳制"), Sim->GetIrMeteringMode(), 2);
	Sim->SetIrMeteringPoint(0.3, 0.7);
	double PointX = 0.0;
	double PointY = 0.0;
	Sim->GetIrMeteringPoint(PointX, PointY);
	TestEqual(TEXT("设置测光点 X"), PointX, 0.3, 1e-9);
	TestEqual(TEXT("设置测光点 Y"), PointY, 0.7, 1e-9);
	Sim->SetIrMeteringPoint(1.5, -0.5);
	Sim->GetIrMeteringPoint(PointX, PointY);
	TestEqual(TEXT("测光点 X 上限钳制"), PointX, 1.0, 1e-9);
	TestEqual(TEXT("测光点 Y 下限钳制"), PointY, 0.0, 1e-9);
	Sim->SetIrMeteringArea(0.4, 0.6, 0.5, 0.5);
	double AreaX = 0.0;
	double AreaY = 0.0;
	double AreaW = 0.0;
	double AreaH = 0.0;
	Sim->GetIrMeteringArea(AreaX, AreaY, AreaW, AreaH);
	TestEqual(TEXT("设置测光区域 X"), AreaX, 0.4, 1e-9);
	TestEqual(TEXT("设置测光区域 Y"), AreaY, 0.6, 1e-9);
	TestEqual(TEXT("设置测光区域宽"), AreaW, 0.5, 1e-9);
	TestEqual(TEXT("设置测光区域高"), AreaH, 0.5, 1e-9);
	Sim->SetIrMeteringArea(0.5, 0.5, 0.0, 2.0);
	Sim->GetIrMeteringArea(AreaX, AreaY, AreaW, AreaH);
	TestEqual(TEXT("测光区域宽下限钳制"), AreaW, 0.01, 1e-9);
	TestEqual(TEXT("测光区域高上限钳制"), AreaH, 1.0, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVCameraStorageSplitTest, "UAV.Payload.CameraSettings.StorageSplit", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVCameraStorageSplitTest::RunTest(const FString& Parameters)
{
	UUAVDroneSimComponent* Sim = NewObject<UUAVDroneSimComponent>();
	TestNotNull(TEXT("组件可创建"), Sim);
	if (!Sim)
	{
		return false;
	}

	// 存储位置：默认 current，空值回落 current
	TestEqual(TEXT("默认照片存储位置"), Sim->GetPhotoStorageLocation(), TEXT("current"));
	TestEqual(TEXT("默认录像存储位置"), Sim->GetVideoStorageLocation(), TEXT("current"));
	Sim->SetPhotoStorageLocation(TEXT("sd_card"));
	Sim->SetVideoStorageLocation(TEXT("sd_card"));
	TestEqual(TEXT("设置照片存储位置"), Sim->GetPhotoStorageLocation(), TEXT("sd_card"));
	TestEqual(TEXT("设置录像存储位置"), Sim->GetVideoStorageLocation(), TEXT("sd_card"));
	Sim->SetPhotoStorageLocation(TEXT(""));
	Sim->SetVideoStorageLocation(TEXT(""));
	TestEqual(TEXT("空照片存储回落"), Sim->GetPhotoStorageLocation(), TEXT("current"));
	TestEqual(TEXT("空录像存储回落"), Sim->GetVideoStorageLocation(), TEXT("current"));

	// 分屏使能
	TestFalse(TEXT("默认未分屏"), Sim->IsScreenSplitEnabled());
	Sim->SetScreenSplitEnabled(true);
	TestTrue(TEXT("开启分屏"), Sim->IsScreenSplitEnabled());
	Sim->SetScreenSplitEnabled(false);
	TestFalse(TEXT("关闭分屏"), Sim->IsScreenSplitEnabled());

	// 焦距：默认 24mm，钳制 1-1000
	TestEqual(TEXT("默认焦距"), Sim->GetFocalLength(), 24.0, 1e-9);
	Sim->SetFocalLength(50.0);
	TestEqual(TEXT("设置焦距"), Sim->GetFocalLength(), 50.0, 1e-9);
	Sim->SetFocalLength(2000.0);
	TestEqual(TEXT("焦距上限钳制"), Sim->GetFocalLength(), 1000.0, 1e-9);
	Sim->SetFocalLength(0.0);
	TestEqual(TEXT("焦距下限钳制"), Sim->GetFocalLength(), 1.0, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVCameraLookAtPoiTest, "UAV.Payload.CameraSettings.LookAtPoi", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVCameraLookAtPoiTest::RunTest(const FString& Parameters)
{
	UUAVDroneSimComponent* Sim = NewObject<UUAVDroneSimComponent>();
	TestNotNull(TEXT("组件可创建"), Sim);
	if (!Sim)
	{
		return false;
	}

	// 看点：设置后可读回，清除后不可读
	FUAVGeoCoordinate Target;
	TestFalse(TEXT("初始无看点目标"), Sim->GetLookAtTarget(Target));
	FUAVGeoCoordinate LookAt;
	LookAt.Latitude = 30.123;
	LookAt.Longitude = 120.456;
	LookAt.Altitude = 100.0;
	Sim->SetLookAtTarget(LookAt);
	TestTrue(TEXT("设置后存在看点目标"), Sim->GetLookAtTarget(Target));
	TestEqual(TEXT("看点纬度"), Target.Latitude, 30.123, 1e-9);
	TestEqual(TEXT("看点经度"), Target.Longitude, 120.456, 1e-9);
	TestEqual(TEXT("看点海拔"), Target.Altitude, 100.0, 1e-9);
	Sim->ClearLookAtTarget();
	TestFalse(TEXT("清除后无看点目标"), Sim->GetLookAtTarget(Target));

	// POI：进入/退出环绕模式，环绕速度钳制为非负
	TestFalse(TEXT("初始未激活 POI"), Sim->IsPoiModeActive());
	Sim->SetPoiModeActive(true);
	TestTrue(TEXT("进入 POI 环绕"), Sim->IsPoiModeActive());
	Sim->SetPoiModeActive(false);
	TestFalse(TEXT("退出 POI 环绕"), Sim->IsPoiModeActive());
	Sim->SetPoiCircleSpeed(8.0, 45.0);
	TestEqual(TEXT("设置 POI 最大速度"), Sim->GetPoiMaxSpeed(), 8.0, 1e-9);
	TestEqual(TEXT("设置 POI 偏航角速度"), Sim->GetPoiGimbalYawRate(), 45.0, 1e-9);
	Sim->SetPoiCircleSpeed(-3.0, -5.0);
	TestEqual(TEXT("POI 速度下限钳制"), Sim->GetPoiMaxSpeed(), 0.0, 1e-9);
	TestEqual(TEXT("POI 角速度下限钳制"), Sim->GetPoiGimbalYawRate(), 0.0, 1e-9);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
