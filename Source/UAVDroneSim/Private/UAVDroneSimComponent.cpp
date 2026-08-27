#include "UAVDroneSimComponent.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"

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

void UUAVDroneSimComponent::UpdateJoystickControl(double DeltaTime)
{
	const double Now = FPlatformTime::Seconds();
	const double ElapsedMs = (Now - LastJoystickCommandTime) * 1000.0;
	FVector TargetVelocity = FVector::ZeroVector;
	if (ElapsedMs <= JoystickDelayTimeMs)
	{
		// 指令有效期内：机体坐标按航向旋转为场景速度（前=+x、右=+y、上=+h）
		const double HeadingRad = FMath::DegreesToRadians(HeadingDegrees);
		const FVector Forward(FMath::Sin(HeadingRad), FMath::Cos(HeadingRad), 0.0);
		const FVector Right(FMath::Cos(HeadingRad), -FMath::Sin(HeadingRad), 0.0);
		TargetVelocity = Forward * (JoystickX * MaxHorizontalSpeed / 17.0)
			+ Right * (JoystickY * MaxHorizontalSpeed / 17.0)
			+ FVector(0.0, 0.0, JoystickH * MaxVerticalSpeed / 5.0);
	}
	// 速度平滑：响应系数随指令频率提升
	const double Smooth = FMath::Clamp(DeltaTime * JoystickFreq, 0.0, 1.0);
	JoystickVelocity += (TargetVelocity - JoystickVelocity) * Smooth;
	// 偏航角速度持续生效（度/秒），归一化到 0-360
	HeadingDegrees = FMath::Fmod(HeadingDegrees + JoystickW * DeltaTime, 360.0);
	if (HeadingDegrees < 0.0)
	{
		HeadingDegrees += 360.0;
	}
	// 位置积分与遥测累计（与航点任务口径一致）
	CurrentLocation += JoystickVelocity * DeltaTime;
	CurrentHorizontalSpeed = FMath::Sqrt(JoystickVelocity.X * JoystickVelocity.X + JoystickVelocity.Y * JoystickVelocity.Y);
	CurrentVerticalSpeed = JoystickVelocity.Z;
	TotalFlightTimeSeconds += DeltaTime;
	TotalFlightDistanceMeters += CurrentHorizontalSpeed * DeltaTime;
	if (IsRecording())
	{
		RecordingTimeSeconds += DeltaTime;
	}
	if (AActor* Owner = GetOwner())
	{
		Owner->SetActorLocation(CurrentLocation);
	}
}

void UUAVDroneSimComponent::SetJoystickCommand(int32 InX, int32 InY, int32 InH, int32 InW, int32 InFreq, int32 InDelayTimeMs)
{
	// 首次进入摇杆控制时停止现有航点任务（DRC 直控与任务互斥）
	if (!bJoystickActive)
	{
		StopMission();
	}
	bJoystickActive = true;
	JoystickX = InX;
	JoystickY = InY;
	JoystickH = InH;
	JoystickW = InW;
	JoystickFreq = FMath::Max(2, InFreq);
	JoystickDelayTimeMs = FMath::Max(100, InDelayTimeMs);
	LastJoystickCommandTime = FPlatformTime::Seconds();
}

void UUAVDroneSimComponent::SetJoystickActive(bool bInActive)
{
	if (bJoystickActive && !bInActive)
	{
		// 停用摇杆控制：清空摇杆速度，恢复航点任务物理分支
		JoystickVelocity = FVector::ZeroVector;
		CurrentHorizontalSpeed = 0.0;
		CurrentVerticalSpeed = 0.0;
	}
	bJoystickActive = bInActive;
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

// ---- 相机设置状态 ----

void UUAVDroneSimComponent::SetExposureMode(int32 NewMode)
{
	ExposureMode = FMath::Clamp(NewMode, 0, 3);
}

void UUAVDroneSimComponent::SetShutterSpeed(double NewShutterSpeed)
{
	// 快门速度下限 1/8000 秒，上限 1 秒
	ShutterSpeed = FMath::Clamp(NewShutterSpeed, 1.0 / 8000.0, 1.0);
}

void UUAVDroneSimComponent::SetIso(int32 NewIso)
{
	Iso = FMath::Clamp(NewIso, 50, 12800);
}

void UUAVDroneSimComponent::SetExposureCompensation(double NewCompensation)
{
	ExposureCompensation = FMath::Clamp(NewCompensation, -3.0, 3.0);
}

void UUAVDroneSimComponent::SetFocusMode(int32 NewMode)
{
	FocusMode = FMath::Clamp(NewMode, 0, 2);
}

void UUAVDroneSimComponent::SetFocusValue(int32 NewValue)
{
	FocusValue = FMath::Clamp(NewValue, 0, 100);
}

void UUAVDroneSimComponent::SetPointFocusAction(const FString& InAction)
{
	PointFocusAction = InAction;
}

void UUAVDroneSimComponent::SetIrMeteringMode(int32 NewMode)
{
	IrMeteringMode = FMath::Clamp(NewMode, 0, 2);
}

void UUAVDroneSimComponent::SetIrMeteringPoint(double InX, double InY)
{
	IrMeteringPointX = FMath::Clamp(InX, 0.0, 1.0);
	IrMeteringPointY = FMath::Clamp(InY, 0.0, 1.0);
}

void UUAVDroneSimComponent::SetIrMeteringArea(double InX, double InY, double InW, double InH)
{
	IrMeteringAreaX = FMath::Clamp(InX, 0.0, 1.0);
	IrMeteringAreaY = FMath::Clamp(InY, 0.0, 1.0);
	IrMeteringAreaW = FMath::Clamp(InW, 0.01, 1.0);
	IrMeteringAreaH = FMath::Clamp(InH, 0.01, 1.0);
}

void UUAVDroneSimComponent::SetPhotoStorageLocation(const FString& InLocation)
{
	PhotoStorageLocation = InLocation.IsEmpty() ? TEXT("current") : InLocation;
}

void UUAVDroneSimComponent::SetVideoStorageLocation(const FString& InLocation)
{
	VideoStorageLocation = InLocation.IsEmpty() ? TEXT("current") : InLocation;
}

void UUAVDroneSimComponent::SetFocalLength(double NewFocalLength)
{
	FocalLength = FMath::Clamp(NewFocalLength, 1.0, 1000.0);
}

bool UUAVDroneSimComponent::GetLookAtTarget(FUAVGeoCoordinate& OutTarget) const
{
	if (!bHasLookAtTarget)
	{
		return false;
	}
	OutTarget = LookAtTarget;
	return true;
}

void UUAVDroneSimComponent::SetLookAtTarget(const FUAVGeoCoordinate& InTarget)
{
	LookAtTarget = InTarget;
	bHasLookAtTarget = true;
}

void UUAVDroneSimComponent::ClearLookAtTarget()
{
	bHasLookAtTarget = false;
}

void UUAVDroneSimComponent::SetPoiCircleSpeed(double InMaxSpeed, double InGimbalYawRate)
{
	PoiMaxSpeed = FMath::Max(0.0, InMaxSpeed);
	PoiGimbalYawRate = FMath::Max(0.0, InGimbalYawRate);
}

void UUAVDroneSimComponent::TakePhoto()
{
	RemainingPhotoNum = FMath::Max(0, RemainingPhotoNum - 1);
	++TakenPhotoCount;
	SetPhotoTaking(true);
}

void UUAVDroneSimComponent::SetPhotoTaking(bool bInPhotoTaking)
{
	bPhotoTaking = bInPhotoTaking;
	// 启动拍照时重置自动结束计时（模拟单张拍摄时长）
	PhotoTakingRemainingSeconds = bInPhotoTaking ? 3.0 : 0.0;
}

void UUAVDroneSimComponent::StartRecording()
{
	bRecordingOverrideSet = true;
	bRecordingOverrideValue = true;
}

void UUAVDroneSimComponent::StopRecording()
{
	bRecordingOverrideSet = true;
	bRecordingOverrideValue = false;
}

void UUAVDroneSimComponent::ClearRecordingOverride()
{
	bRecordingOverrideSet = false;
	bRecordingOverrideValue = false;
}

void UUAVDroneSimComponent::SetGimbalTarget(double InPitchDegrees, double InYawDegrees)
{
	bHasGimbalTarget = true;
	GimbalTargetPitch = InPitchDegrees;
	GimbalTargetYaw = InYawDegrees;
}

void UUAVDroneSimComponent::ResetGimbalTarget()
{
	bHasGimbalTarget = false;
	GimbalTargetPitch = 0.0;
	GimbalTargetYaw = 0.0;
}

bool UUAVDroneSimComponent::IsRecording() const
{
	// 指令覆盖优先，否则按飞行模式推导
	if (bRecordingOverrideSet)
	{
		return bRecordingOverrideValue;
	}
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
	if (bHasGimbalTarget)
	{
		// 云台指令目标优先于时间微动
		GimbalState = UAVPayloadMath::ApplyGimbalTarget(GimbalState, GimbalTargetPitch, GimbalTargetYaw);
	}

	// 拍照自动结束：拍照中状态持续 3 秒后复位
	if (bPhotoTaking)
	{
		PhotoTakingRemainingSeconds -= DeltaTime;
		if (PhotoTakingRemainingSeconds <= 0.0)
		{
			bPhotoTaking = false;
			PhotoTakingRemainingSeconds = 0.0;
		}
	}

	if (bMissionActive && !bMissionPaused)
	{
		TotalFlightTimeSeconds += DeltaTime;
		TotalFlightDistanceMeters += CurrentHorizontalSpeed * DeltaTime;
		if (IsRecording())
		{
			RecordingTimeSeconds += DeltaTime;
		}
	}

	// 摇杆直控模式：优先于航点任务推进（DRC 指令过期后目标归零悬停）
	if (bJoystickActive)
	{
		UpdateJoystickControl(DeltaTime);
		return;
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
