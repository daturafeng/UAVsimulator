// 地理坐标工具实现
#include "UAVGeoUtils.h"

FVector UUAVGeoUtils::LatLonAltToScene(const FUAVGeoCoordinate& InAirportOrigin, const FUAVGeoCoordinate& InCoord)
{
	const double EastMeters = LonDeltaToEastMeters(InAirportOrigin.Latitude, InCoord.Longitude - InAirportOrigin.Longitude);
	const double NorthMeters = LatDeltaToNorthMeters(InCoord.Latitude - InAirportOrigin.Latitude);
	const double UpMeters = InCoord.Altitude - InAirportOrigin.Altitude;
	return FVector(EastMeters, NorthMeters, UpMeters);
}

FUAVGeoCoordinate UUAVGeoUtils::SceneToLatLonAlt(const FUAVGeoCoordinate& InAirportOrigin, const FVector& InSceneLocation)
{
	FUAVGeoCoordinate Result;
	Result.Longitude = InAirportOrigin.Longitude + EastMetersToLonDelta(InAirportOrigin.Latitude, InSceneLocation.X);
	Result.Latitude = InAirportOrigin.Latitude + NorthMetersToLatDelta(InSceneLocation.Y);
	Result.Altitude = InAirportOrigin.Altitude + InSceneLocation.Z;
	return Result;
}

double UUAVGeoUtils::LonDeltaToEastMeters(double InAirportLatitudeDeg, double InLonDeltaDeg)
{
	// 与 dock common.py：经度米 = 经度差 * 111320 * cos(纬度)
	return InLonDeltaDeg * MetersPerLonDegreeAtEquator * FMath::Cos(FMath::DegreesToRadians(InAirportLatitudeDeg));
}

double UUAVGeoUtils::LatDeltaToNorthMeters(double InLatDeltaDeg)
{
	// 与 dock common.py：纬度米 = 纬度差 * 110540
	return InLatDeltaDeg * MetersPerLatDegree;
}

double UUAVGeoUtils::EastMetersToLonDelta(double InAirportLatitudeDeg, double InEastMeters)
{
	// 逆换算；cos 下限 0.2 防止高纬度分母过小（与 dock common.py 一致）
	const double CosLat = FMath::Max(0.2, FMath::Cos(FMath::DegreesToRadians(InAirportLatitudeDeg)));
	return InEastMeters / (MetersPerLonDegreeAtEquator * CosLat);
}

double UUAVGeoUtils::NorthMetersToLatDelta(double InNorthMeters)
{
	return InNorthMeters / MetersPerLatDegree;
}

double UUAVGeoUtils::DistanceMeters(const FUAVGeoCoordinate& A, const FUAVGeoCoordinate& B)
{
	// 与 dock common.py：使用中点纬度计算经度比例
	const double MidLatitude = (A.Latitude + B.Latitude) * 0.5;
	const double EastMeters = LonDeltaToEastMeters(MidLatitude, B.Longitude - A.Longitude);
	const double NorthMeters = LatDeltaToNorthMeters(B.Latitude - A.Latitude);
	return FMath::Sqrt(EastMeters * EastMeters + NorthMeters * NorthMeters);
}
