// 地理坐标工具：经纬度/海拔 ↔ UE 场景坐标（机场为原点 ENU）
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UAVGeoUtils.generated.h"

/** 地理坐标（WGS84 经纬度 + 海拔） */
USTRUCT(BlueprintType)
struct FUAVGeoCoordinate
{
	GENERATED_BODY()

	/** 纬度（度，北纬为正） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Geo")
	double Latitude = 0.0;

	/** 经度（度，东经为正） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Geo")
	double Longitude = 0.0;

	/** 海拔（米，相对海平面） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Geo")
	double Altitude = 0.0;
};

/**
 * 经纬度/海拔与 UE 场景坐标的双向转换工具。
 *
 * 局部坐标系：以机场为原点，场景轴约定 X=东、Y=北、Z=上（米制），
 * 与 UE 默认轴约定不同，所有换算集中在单点实现，避免混淆。
 * 换算口径与 dock 项目 script/common.py 的等距近似公式一致，
 * 适用边界：机场周边数公里内；远距离（>10km）误差会累积。
 */
UCLASS()
class UAVDRONESIM_API UUAVGeoUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 纬度 1° 对应的米数 */
	static constexpr double MetersPerLatDegree = 110540.0;

	/** 赤道处经度 1° 对应的米数（实际按纬度乘以 cos(lat)） */
	static constexpr double MetersPerLonDegreeAtEquator = 111320.0;

	/** 经纬度/海拔 → 场景坐标（相对机场原点，X=东、Y=北、Z=上） */
	UFUNCTION(BlueprintCallable, Category = "UAV|Geo")
	static FVector LatLonAltToScene(const FUAVGeoCoordinate& InAirportOrigin, const FUAVGeoCoordinate& InCoord);

	/** 场景坐标 → 经纬度/海拔（与 LatLonAltToScene 互逆） */
	UFUNCTION(BlueprintCallable, Category = "UAV|Geo")
	static FUAVGeoCoordinate SceneToLatLonAlt(const FUAVGeoCoordinate& InAirportOrigin, const FVector& InSceneLocation);

	/** 两点（经纬度）间水平距离（米，等距近似） */
	UFUNCTION(BlueprintCallable, Category = "UAV|Geo")
	static double DistanceMeters(const FUAVGeoCoordinate& A, const FUAVGeoCoordinate& B);

	/** 经度差（度）→ 东向米（按给定纬度） */
	static double LonDeltaToEastMeters(double InAirportLatitudeDeg, double InLonDeltaDeg);

	/** 纬度差（度）→ 北向米 */
	static double LatDeltaToNorthMeters(double InLatDeltaDeg);

	/** 东向米 → 经度差（度，按给定纬度） */
	static double EastMetersToLonDelta(double InAirportLatitudeDeg, double InEastMeters);

	/** 北向米 → 纬度差（度） */
	static double NorthMetersToLatDelta(double InNorthMeters);
};
