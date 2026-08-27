// 载荷状态推导纯函数：电量/电池/云台/风向（对齐 dock report_drone_osd.py 口径）
#pragma once

#include "CoreMinimal.h"
#include "UAVPayloadMath.generated.h"

/** 云台模拟配置 */
USTRUCT(BlueprintType)
struct FUAVGimbalConfig
{
	GENERATED_BODY()

	/** 俯仰基础角（度，对齐 dock -8 基线） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Gimbal")
	double PitchBaseDegrees = -8.0;

	/** 俯仰正弦振幅（度） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Gimbal", meta = (ClampMin = "0.0"))
	double PitchAmplitudeDegrees = 6.0;

	/** 横滚正弦振幅（度） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Gimbal", meta = (ClampMin = "0.0"))
	double RollAmplitudeDegrees = 1.2;

	/** 偏航是否跟随机头朝向 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Gimbal")
	bool bYawFollowsHeading = true;

	/** 偏航跟随时的微动振幅（度） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Gimbal", meta = (ClampMin = "0.0"))
	double YawSwayAmplitudeDegrees = 4.0;
};

/** 云台角度状态 */
USTRUCT(BlueprintType)
struct FUAVGimbalState
{
	GENERATED_BODY()

	/** 俯仰角（度） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Gimbal")
	double PitchDegrees = 0.0;

	/** 横滚角（度） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Gimbal")
	double RollDegrees = 0.0;

	/** 偏航角（度，0=北、顺时针） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Gimbal")
	double YawDegrees = 0.0;
};

/** 载荷状态推导纯函数（无组件依赖，可单元测试） */
namespace UAVPayloadMath
{
	/** 电量消耗推进：飞行/待机按不同速率，结果钳制到 [0,100] */
	UAVDRONESIM_API double DrainBatteryPercent(double InCapacityPercent, bool bInFlight, double InDeltaSeconds,
		double InFlightDrainPercentPerSecond, double InIdleDrainPercentPerSecond);

	/** 剩余飞行时间（秒）：电量 / 飞行消耗速率；速率为 0 时返回 0 */
	UAVDRONESIM_API double EstimateRemainFlightTimeSeconds(double InCapacityPercent, double InFlightDrainPercentPerSecond);

	/** 电池单元温度（摄氏度），对齐 dock：27.5 + (100-电量)*0.06 + 水平速度*0.18，保留 1 位小数 */
	UAVDRONESIM_API double ComputeBatteryTemperatureCelsius(double InCapacityPercent, double InHorizontalSpeed);

	/** 电池单元电压（毫伏），对齐 dock：max(22000, 25800 + 电量*18 - 水平速度*35) */
	UAVDRONESIM_API int32 ComputeBatteryVoltageMv(double InCapacityPercent, double InHorizontalSpeed);

	/** 云台角度：俯仰/横滚随时间正弦微动，偏航可选跟随机头（含微动），角度归一化到 0-360 */
	UAVDRONESIM_API FUAVGimbalState ComputeGimbalState(double InHeadingDegrees, double InElapsedSeconds, const FUAVGimbalConfig& InConfig);

	/** 风向 8 方位枚举（dock 口径：输入已按 (朝向+180) 归一化，0=北，顺时针 45° 步进，返回 0-7） */
	UAVDRONESIM_API int32 ComputeWindDirectionEnum(double InDegrees);

	/** 角度归一化到 [0,360) */
	UAVDRONESIM_API double NormalizeDegrees(double InDegrees);
}
