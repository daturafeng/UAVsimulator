#include "UAVDroneSimComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	/** 航点 → 地理坐标（两者字段相同，类型不同，显式转换） */
	FUAVGeoCoordinate WaypointToGeo(const FUAVWaypoint& InWaypoint)
	{
		FUAVGeoCoordinate Result;
		Result.Latitude = InWaypoint.Latitude;
		Result.Longitude = InWaypoint.Longitude;
		Result.Altitude = InWaypoint.Altitude;
		return Result;
	}
}

UUAVDroneSimComponent::UUAVDroneSimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UUAVDroneSimComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		CurrentLocation = Owner->GetActorLocation();
		TargetLocation = CurrentLocation;
	}
	BatteryCapacityPercent = BatteryCapacityStartPercent;
	ZoomFactor = DefaultZoomFactor;
}

FUAVGeoCoordinate UUAVDroneSimComponent::GetCurrentGeoCoordinate() const
{
	return UUAVGeoUtils::SceneToLatLonAlt(AirportOrigin, CurrentLocation);
}

bool UUAVDroneSimComponent::GetCurrentTargetScene(FVector& OutTarget) const
{
	if (!bMissionActive || bMissionPaused)
	{
		return false;
	}
	if (bDirectMove)
	{
		OutTarget = TargetLocation;
		return true;
	}
	if (CurrentWaypointIndex < Waypoints.Num())
	{
		OutTarget = UUAVGeoUtils::LatLonAltToScene(AirportOrigin, WaypointToGeo(Waypoints[CurrentWaypointIndex]));
		return true;
	}
	return false;
}

double UUAVDroneSimComponent::GetRemainingDistance() const
{
	FVector Target;
	if (!GetCurrentTargetScene(Target))
	{
		return 0.0;
	}
	return FVector::Distance(Target, CurrentLocation);
}

double UUAVDroneSimComponent::GetRemainingMissionDistance() const
{
	if (bDirectMove)
	{
		return GetRemainingDistance();
	}
	if (!bMissionActive || CurrentWaypointIndex >= Waypoints.Num())
	{
		return 0.0;
	}
	double Total = 0.0;
	FVector Prev = CurrentLocation;
	for (int32 Index = CurrentWaypointIndex; Index < Waypoints.Num(); ++Index)
	{
		const FVector Next = UUAVGeoUtils::LatLonAltToScene(AirportOrigin, WaypointToGeo(Waypoints[Index]));
		Total += FVector::Distance(Next, Prev);
		Prev = Next;
	}
	return Total;
}

int32 UUAVDroneSimComponent::GetCurrentWaypointIndex() const
{
	if (!bMissionActive || bDirectMove || Waypoints.Num() == 0)
	{
		return -1;
	}
	return CurrentWaypointIndex;
}

bool UUAVDroneSimComponent::SetWaypoints(const TArray<FUAVWaypoint>& InWaypoints, bool bStartImmediately)
{
	Waypoints = InWaypoints;
	CurrentWaypointIndex = 0;
	bDirectMove = false;
	bMissionPaused = false;
	bMissionActive = false;
	if (bStartImmediately)
	{
		return StartMission();
	}
	return Waypoints.Num() > 0;
}

void UUAVDroneSimComponent::AddWaypoint(const FUAVWaypoint& InWaypoint)
{
	Waypoints.Add(InWaypoint);
}

void UUAVDroneSimComponent::ClearWaypoints()
{
	Waypoints.Reset();
	CurrentWaypointIndex = 0;
	bMissionActive = false;
	bMissionPaused = false;
	bDirectMove = false;
}

bool UUAVDroneSimComponent::StartMission()
{
	if (Waypoints.Num() == 0 && !bDirectMove)
	{
		return false;
	}
	CurrentWaypointIndex = 0;
	bMissionPaused = false;
	bMissionActive = true;
	return true;
}

void UUAVDroneSimComponent::PauseMission()
{
	bMissionPaused = true;
}

void UUAVDroneSimComponent::ResumeMission()
{
	bMissionPaused = false;
}

void UUAVDroneSimComponent::StopMission()
{
	bMissionActive = false;
	bMissionPaused = false;
	CurrentWaypointIndex = 0;
	bDirectMove = false;
}

void UUAVDroneSimComponent::MoveToLocation(const FVector& NewLocation)
{
	TargetLocation = NewLocation;
	bDirectMove = true;
	bMissionPaused = false;
	CurrentWaypointIndex = 0;
	bMissionActive = true;
}

void UUAVDroneSimComponent::UpdateHeading(const FVector& InHorizontalDir)
{
	// 场景轴 X=东、Y=北：atan2(东, 北)，0=北、顺时针
	HeadingDegrees = FMath::RadiansToDegrees(FMath::Atan2(InHorizontalDir.X, InHorizontalDir.Y));
}

double UUAVDroneSimComponent::GetArrivalThreshold() const
{
	if (!bDirectMove && CurrentWaypointIndex < Waypoints.Num())
	{
		const double Threshold = Waypoints[CurrentWaypointIndex].ArrivalThresholdMeters;
		if (Threshold > 0.0)
		{
			return Threshold;
		}
	}
	return DefaultArrivalThresholdMeters;
}

double UUAVDroneSimComponent::GetRemainFlightTimeSeconds() const
{
	return UAVPayloadMath::EstimateRemainFlightTimeSeconds(BatteryCapacityPercent, BatteryDrainPercentPerSecond);
}

void UUAVDroneSimComponent::GetBatteryCell(int32 InIndex, double& OutTemperatureCelsius, int32& OutVoltageMv) const
{
	// 对齐 dock 双电池：第二单元温度 +0.5、电压 -80
	OutTemperatureCelsius = UAVPayloadMath::ComputeBatteryTemperatureCelsius(BatteryCapacityPercent, CurrentHorizontalSpeed)
		+ (InIndex == 1 ? 0.5 : 0.0);
	OutVoltageMv = UAVPayloadMath::ComputeBatteryVoltageMv(BatteryCapacityPercent, CurrentHorizontalSpeed)
		+ (InIndex == 1 ? -80 : 0);
}

void UUAVDroneSimComponent::SetCameraMode(int32 NewMode)
{
	CameraMode = FMath::Clamp(NewMode, 0, 1);
}

void UUAVDroneSimComponent::SetZoomFactor(double NewZoomFactor)
{
	ZoomFactor = FMath::Clamp(NewZoomFactor, ZoomFactorMin, FMath::Max(ZoomFactorMin, ZoomFactorMax));
}

bool UUAVDroneSimComponent::IsRecording() const
{
	// 对齐 dock：模式编码 ∈ {1,4,9} 即 起飞/航线/返航 时录像中
	return FlightState == EUAVFlightState::TakingOff
		|| FlightState == EUAVFlightState::Wayline
		|| FlightState == EUAVFlightState::ReturnHome;
}

void UUAVDroneSimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (DeltaTime <= 0.0f)
	{
		return;
	}

	// ---- 载荷状态推进（与移动状态无关） ----
	const bool bInFlight = (FlightState != EUAVFlightState::Idle);
	BatteryCapacityPercent = UAVPayloadMath::DrainBatteryPercent(
		BatteryCapacityPercent, bInFlight, DeltaTime, BatteryDrainPercentPerSecond, BatteryIdleDrainPercentPerSecond);
	if (!bBatteryLowEventFired && BatteryCapacityPercent <= BatteryReturnHomePowerPercent)
	{
		bBatteryLowEventFired = true;
		OnBatteryLow.Broadcast(BatteryCapacityPercent);
	}
	else if (bBatteryLowEventFired && BatteryCapacityPercent > BatteryReturnHomePowerPercent)
	{
		// 电量回升后复位，允许再次触发
		bBatteryLowEventFired = false;
	}

	ElapsedSimTimeSeconds += DeltaTime;
	GimbalState = UAVPayloadMath::ComputeGimbalState(HeadingDegrees, ElapsedSimTimeSeconds, GimbalConfig);

	if (bMissionActive && !bMissionPaused)
	{
		TotalFlightTimeSeconds += DeltaTime;
		TotalFlightDistanceMeters += CurrentHorizontalSpeed * DeltaTime;
		if (IsRecording())
		{
			RecordingTimeSeconds += DeltaTime;
		}
	}

	// 无进行中任务或已暂停：位置保持，速度归零
	if (!bMissionActive || bMissionPaused)
	{
		CurrentHorizontalSpeed = 0.0;
		CurrentVerticalSpeed = 0.0;
		return;
	}

	FVector Target;
	if (!GetCurrentTargetScene(Target))
	{
		bMissionActive = false;
		CurrentHorizontalSpeed = 0.0;
		CurrentVerticalSpeed = 0.0;
		return;
	}

	const FVector Delta = Target - CurrentLocation;
	const double HorizontalDist = FMath::Sqrt(Delta.X * Delta.X + Delta.Y * Delta.Y);

	// 水平移动：按最大水平速度钳制
	const double MaxHorizStep = MaxHorizontalSpeed * DeltaTime;
	if (HorizontalDist > MaxHorizStep)
	{
		const FVector Dir2D(Delta.X / HorizontalDist, Delta.Y / HorizontalDist, 0.0);
		CurrentLocation.X += Dir2D.X * MaxHorizStep;
		CurrentLocation.Y += Dir2D.Y * MaxHorizStep;
		CurrentHorizontalSpeed = MaxHorizontalSpeed;
		UpdateHeading(Dir2D);
	}
	else
	{
		CurrentLocation.X = Target.X;
		CurrentLocation.Y = Target.Y;
		CurrentHorizontalSpeed = HorizontalDist / DeltaTime;
	}

	// 垂直移动：按最大垂直速度钳制（可原地爬升/下降）
	const double VertDelta = Target.Z - CurrentLocation.Z;
	const double MaxVertStep = MaxVerticalSpeed * DeltaTime;
	if (FMath::Abs(VertDelta) > MaxVertStep)
	{
		CurrentLocation.Z += FMath::Sign(VertDelta) * MaxVertStep;
		CurrentVerticalSpeed = MaxVerticalSpeed * FMath::Sign(VertDelta);
	}
	else
	{
		CurrentLocation.Z = Target.Z;
		CurrentVerticalSpeed = VertDelta / DeltaTime;
	}

	if (AActor* Owner = GetOwner())
	{
		Owner->SetActorLocation(CurrentLocation);
	}

	// 到达判定：距离当前目标小于阈值
	const double Remaining = FVector::Distance(Target, CurrentLocation);
	if (Remaining <= GetArrivalThreshold())
	{
		if (bDirectMove)
		{
			bMissionActive = false;
			bDirectMove = false;
			CurrentHorizontalSpeed = 0.0;
			CurrentVerticalSpeed = 0.0;
			OnMissionFinished.Broadcast();
		}
		else
		{
			const int32 ReachedIndex = CurrentWaypointIndex;
			++CurrentWaypointIndex;
			OnWaypointReached.Broadcast(ReachedIndex);
			if (CurrentWaypointIndex >= Waypoints.Num())
			{
				bMissionActive = false;
				CurrentHorizontalSpeed = 0.0;
				CurrentVerticalSpeed = 0.0;
				OnMissionFinished.Broadcast();
			}
		}
	}
}
