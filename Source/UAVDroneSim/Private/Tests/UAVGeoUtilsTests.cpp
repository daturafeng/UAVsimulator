// UAVGeoUtils 自动化测试：已知点校验与互逆性验证（任务 2.2）
#include "Misc/AutomationTest.h"
#include "UAVGeoUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVGeoUtilsConversionTest, "UAV.Geo.Conversion", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVGeoUtilsConversionTest::RunTest(const FString& Parameters)
{
	// 机场原点：与 dock 假设备 DOCK3TEST001 口径一致
	FUAVGeoCoordinate Airport;
	Airport.Latitude = 29.55696;
	Airport.Longitude = 106.57668;
	Airport.Altitude = 300.0;

	// 已知点 1：正北 0.01°（≈1105.4m），海拔 +20m
	FUAVGeoCoordinate TargetNorth;
	TargetNorth.Latitude = 29.56696;
	TargetNorth.Longitude = 106.57668;
	TargetNorth.Altitude = 320.0;

	const FVector SceneNorth = UUAVGeoUtils::LatLonAltToScene(Airport, TargetNorth);
	TestEqual(TEXT("正北偏移米"), SceneNorth.Y, 1105.4, 1.0);
	TestEqual(TEXT("东向应接近 0"), SceneNorth.X, 0.0, 0.01);
	TestEqual(TEXT("高度差"), SceneNorth.Z, 20.0, 0.01);

	// 已知点 2：正东 0.01°（≈111320*cos(29.55696°)*0.01 ≈ 968.3m）
	FUAVGeoCoordinate TargetEast;
	TargetEast.Latitude = 29.55696;
	TargetEast.Longitude = 106.58668;
	TargetEast.Altitude = 300.0;

	const FVector SceneEast = UUAVGeoUtils::LatLonAltToScene(Airport, TargetEast);
	TestEqual(TEXT("正东偏移米"), SceneEast.X, 968.33, 1.0);
	TestEqual(TEXT("北向应接近 0"), SceneEast.Y, 0.0, 0.01);

	// 互逆性：场景坐标 → 经纬度 → 场景坐标，误差在毫米级
	const FUAVGeoCoordinate RoundTrip = UUAVGeoUtils::SceneToLatLonAlt(Airport, SceneEast);
	const FVector RoundTripScene = UUAVGeoUtils::LatLonAltToScene(Airport, RoundTrip);
	TestEqual(TEXT("互逆东向"), RoundTripScene.X, SceneEast.X, 1e-3);
	TestEqual(TEXT("互逆北向"), RoundTripScene.Y, SceneEast.Y, 1e-3);
	TestEqual(TEXT("互逆高度"), RoundTripScene.Z, SceneEast.Z, 1e-3);

	// 距离：两点水平距离（约 1459m，勾股：sqrt(1105.4^2 + 968.3^2)）
	const double Dist = UUAVGeoUtils::DistanceMeters(TargetNorth, TargetEast);
	TestEqual(TEXT("两点水平距离"), Dist, 1469.9, 2.0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
