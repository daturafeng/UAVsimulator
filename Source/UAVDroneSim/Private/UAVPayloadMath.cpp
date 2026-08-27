#include "UAVPayloadMath.h"

namespace
{
	/** dock 电池温度基线公式（含微动的基线部分） */
	constexpr double kBatteryTemperatureBase = 27.5;
	constexpr double kBatteryTemperatureCapacityFactor = 0.06;
	constexpr double kBatteryTemperatureSpeedFactor = 0.18;

	/** dock 电池电压基线公式 */
	constexpr int32 kBatteryVoltageBaseMv = 25800;
	constexpr double kBatteryVoltageCapacityFactor = 18.0;
	constexpr double kBatteryVoltageSpeedFactor = 35.0;
	constexpr int32 kBatteryVoltageFloorMv = 22000;
}

namespace UAVPayloadMath
{
	double DrainBatteryPercent(double InCapacityPercent, bool bInFlight, double InDeltaSeconds,
		double InFlightDrainPercentPerSecond, double InIdleDrainPercentPerSecond)
	{
		if (InDeltaSeconds <= 0.0)
		{
			return FMath::Clamp(InCapacityPercent, 0.0, 100.0);
		}
		const double Rate = bInFlight ? InFlightDrainPercentPerSecond : InIdleDrainPercentPerSecond;
		return FMath::Clamp(InCapacityPercent - Rate * InDeltaSeconds, 0.0, 100.0);
	}

	double EstimateRemainFlightTimeSeconds(double InCapacityPercent, double InFlightDrainPercentPerSecond)
	{
		if (InFlightDrainPercentPerSecond <= 0.0)
		{
			return 0.0;
		}
		return FMath::Max(0.0, InCapacityPercent) / InFlightDrainPercentPerSecond;
	}

	double ComputeBatteryTemperatureCelsius(double InCapacityPercent, double InHorizontalSpeed)
	{
		const double Temperature = kBatteryTemperatureBase
			+ (100.0 - FMath::Clamp(InCapacityPercent, 0.0, 100.0)) * kBatteryTemperatureCapacityFactor
			+ InHorizontalSpeed * kBatteryTemperatureSpeedFactor;
		return FMath::RoundToDouble(Temperature * 10.0) / 10.0;
	}

	int32 ComputeBatteryVoltageMv(double InCapacityPercent, double InHorizontalSpeed)
	{
		const double Voltage = kBatteryVoltageBaseMv
			+ FMath::Clamp(InCapacityPercent, 0.0, 100.0) * kBatteryVoltageCapacityFactor
			- InHorizontalSpeed * kBatteryVoltageSpeedFactor;
		return FMath::Max(kBatteryVoltageFloorMv, FMath::RoundToInt(Voltage));
	}

	double NormalizeDegrees(double InDegrees)
	{
		double Normalized = FMath::Fmod(InDegrees, 360.0);
		if (Normalized < 0.0)
		{
			Normalized += 360.0;
		}
		return Normalized;
	}

	FUAVGimbalState ComputeGimbalState(double InHeadingDegrees, double InElapsedSeconds, const FUAVGimbalConfig& InConfig)
	{
		FUAVGimbalState Result;
		Result.PitchDegrees = InConfig.PitchBaseDegrees + FMath::Sin(InElapsedSeconds * 0.24) * InConfig.PitchAmplitudeDegrees;
		Result.RollDegrees = FMath::Cos(InElapsedSeconds * 0.22) * InConfig.RollAmplitudeDegrees;
		if (InConfig.bYawFollowsHeading)
		{
			Result.YawDegrees = NormalizeDegrees(InHeadingDegrees + FMath::Sin(InElapsedSeconds * 0.28) * InConfig.YawSwayAmplitudeDegrees);
		}
		else
		{
			Result.YawDegrees = NormalizeDegrees(InHeadingDegrees);
		}
		return Result;
	}

	int32 ComputeWindDirectionEnum(double InDegrees)
	{
		const double Normalized = NormalizeDegrees(InDegrees);
		// dock 口径：0=北（0-22.5 与 337.5-360），顺时针每 45° 递增
		if (Normalized < 22.5 || Normalized >= 337.5)
		{
			return 0;
		}
		if (Normalized < 67.5)
		{
			return 1;
		}
		if (Normalized < 112.5)
		{
			return 2;
		}
		if (Normalized < 157.5)
		{
			return 3;
		}
		if (Normalized < 202.5)
		{
			return 4;
		}
		if (Normalized < 247.5)
		{
			return 5;
		}
		if (Normalized < 292.5)
		{
			return 6;
		}
		return 7;
	}
}
