#include "UAVFlightControlComponent.h"
#include "UAVCloudApiTypes.h"
#include "UAVGeoUtils.h"

#include "Serialization/JsonSerializer.h"

namespace
{
	/** 解析 JSON 字符串为对象；失败返回 false（空字符串视为无参数） */
	bool TryParseJsonObject(const FString& InJson, TSharedPtr<FJsonObject>& OutObject)
	{
		if (InJson.IsEmpty())
		{
			return false;
		}
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InJson);
		return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
	}

	/** 安全读取数字字段 */
	double ReadNumber(const TSharedPtr<FJsonObject>& InData, const TCHAR* InKey, double InDefault = 0.0)
	{
		double Value = InDefault;
		if (InData.IsValid())
		{
			InData->TryGetNumberField(InKey, Value);
		}
		return Value;
	}

	/** 安全读取字符串字段 */
	FString ReadString(const TSharedPtr<FJsonObject>& InData, const TCHAR* InKey, const TCHAR* InDefault = TEXT(""))
	{
		FString Value = InDefault;
		if (InData.IsValid())
		{
			InData->TryGetStringField(InKey, Value);
		}
		return Value;
	}

/**
 * 解析指点飞行目标点（dock Point：latitude/longitude/height，height 2-10000）。
 * points 数组取首个点（M30 系列仅支持单点），解析成功返回 true 并填充 OutTarget。
 */
bool ParseFlyToTarget(const TSharedPtr<FJsonObject>& InData, FUAVWaypoint& OutTarget)
{
	if (!InData.IsValid()) return false;
	const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
	if (!InData->TryGetArrayField(TEXT("points"), Points) || Points->Num() == 0) return false;
	const TSharedPtr<FJsonObject> Point = (*Points)[0]->AsObject();
	if (!Point.IsValid()) return false;

	double Latitude = 0.0;
	double Longitude = 0.0;
	double Height = 0.0;
	if (!Point->TryGetNumberField(TEXT("latitude"), Latitude)) return false;
	if (!Point->TryGetNumberField(TEXT("longitude"), Longitude)) return false;
	if (!Point->TryGetNumberField(TEXT("height"), Height)) return false;
	if (Latitude < -90.0 || Latitude > 90.0) return false;
	if (Longitude < -180.0 || Longitude > 180.0) return false;
	if (Height < 2.0 || Height > 10000.0) return false;

	OutTarget.Latitude = Latitude;
	OutTarget.Longitude = Longitude;
	OutTarget.Altitude = Height;
	OutTarget.ArrivalThresholdMeters = 1.5;
	return true;
}
}

UUAVFlightControlComponent::UUAVFlightControlComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UUAVFlightControlComponent::BeginPlay()
{
	Super::BeginPlay();
	if (DroneSim)
	{
		DroneSim->OnWaypointReached.AddDynamic(this, &UUAVFlightControlComponent::OnDroneWaypointReached);
		DroneSim->OnMissionFinished.AddDynamic(this, &UUAVFlightControlComponent::OnDroneMissionFinished);
		DroneSim->OnBatteryLow.AddDynamic(this, &UUAVFlightControlComponent::OnDroneBatteryLow);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[UAVFlightControl] DroneSim 未注入，飞行指令将无法执行"));
	}
}

void UUAVFlightControlComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (DroneSim)
	{
		DroneSim->OnWaypointReached.RemoveDynamic(this, &UUAVFlightControlComponent::OnDroneWaypointReached);
		DroneSim->OnMissionFinished.RemoveDynamic(this, &UUAVFlightControlComponent::OnDroneMissionFinished);
		DroneSim->OnBatteryLow.RemoveDynamic(this, &UUAVFlightControlComponent::OnDroneBatteryLow);
	}
	Super::EndPlay(EndPlayReason);
}

void UUAVFlightControlComponent::SetDroneSim(UUAVDroneSimComponent* InDroneSim)
{
	DroneSim = InDroneSim;
}

int32 UUAVFlightControlComponent::HandleCommand(const FString& InMethod, const FString& InDataJson)
{
	using namespace UAV::CloudApi;
	using namespace UAV::FlightControlResult;

	TSharedPtr<FJsonObject> Data;
	TryParseJsonObject(InDataJson, Data);

	const FString Method = InMethod.TrimStartAndEnd();
	int32 Result = InternalError;
	if (Method == kMethodFlightAuthorityGrab)
	{
		Result = HandleAuthorityGrab(Data);
	}
	else if (Method == kMethodTakeoffToPoint)
	{
		Result = HandleTakeoffToPoint(Data);
	}
	else if (Method == kMethodFlighttaskCreate)
	{
		Result = HandleFlighttaskCreate(Data);
	}
	else if (Method == kMethodFlighttaskPrepare)
	{
		Result = HandleFlighttaskPrepare(Data);
	}
	else if (Method == kMethodFlighttaskExecute)
	{
		Result = HandleFlighttaskExecute(Data);
	}
	else if (Method == kMethodFlighttaskUndo)
	{
		Result = HandleFlighttaskUndo(Data);
	}
	else if (Method == kMethodFlighttaskPause)
	{
		Result = HandleFlighttaskPause(Data);
	}
	else if (Method == kMethodFlighttaskRecovery)
	{
		Result = HandleFlighttaskRecovery(Data);
	}
	else if (Method == kMethodReturnHome)
	{
		Result = HandleReturnHome(Data);
	}
	else if (Method == kMethodReturnHomeCancel)
	{
		Result = HandleReturnHomeCancel(Data);
	}
	else if (Method == kMethodFlyToPoint)
	{
		Result = HandleFlyToPoint(Data);
	}
	else if (Method == kMethodFlyToPointStop)
	{
		Result = HandleFlyToPointStop(Data);
	}
	else if (Method == kMethodFlyToPointUpdate)
	{
		Result = HandleFlyToPointUpdate(Data);
	}
	else if (Method == kMethodDrcModeEnter)
	{
		Result = HandleDrcModeEnter(Data);
	}
	else if (Method == kMethodDrcModeExit)
	{
		Result = HandleDrcModeExit(Data);
	}
	else if (Method == kMethodDroneControl)
	{
		Result = HandleDroneControl(Data);
	}
	else if (Method == kMethodHeartBeat)
	{
		Result = HandleHeartBeat(Data);
	}
	else if (Method == kMethodDroneEmergencyStop)
	{
		Result = HandleDroneEmergencyStop(Data);
	}
	else
	{
		Result = UnknownMethod;
	}

	OnCommandResult.Broadcast(Method, Result);
	return Result;
}

void UUAVFlightControlComponent::SetWaylineWaypoints(const FString& InFlightId, const TArray<FUAVWaypoint>& InWaypoints)
{
	FUAVMissionEntry& Entry = Missions.FindOrAdd(InFlightId);
	Entry.FlightId = InFlightId;
	Entry.Waypoints = InWaypoints;
}

int32 UUAVFlightControlComponent::HandleAuthorityGrab(const TSharedPtr<FJsonObject>& InData)
{
	bHasFlightAuthority = true;
	UE_LOG(LogTemp, Log, TEXT("[UAVFlightControl] 飞行权已抢占"));
	return UAV::FlightControlResult::Success;
}

int32 UUAVFlightControlComponent::HandleTakeoffToPoint(const TSharedPtr<FJsonObject>& InData)
{
	using namespace UAV::FlightControlResult;
	if (!DroneSim) return InternalError;
	if (!bHasFlightAuthority) return NoAuthority;
	if (FlightState == EUAVFlightState::TakingOff || FlightState == EUAVFlightState::Wayline
		|| FlightState == EUAVFlightState::ReturnHome || FlightState == EUAVFlightState::Landing)
	{
		return StateConflict;
	}

	const FString FlightId = ReadString(InData, TEXT("flight_id"));
	const double TargetLongitude = ReadNumber(InData, TEXT("target_longitude"));
	const double TargetLatitude = ReadNumber(InData, TEXT("target_latitude"));
	const double TargetHeight = ReadNumber(InData, TEXT("target_height"));
	const double SecurityTakeoffHeight = ReadNumber(InData, TEXT("security_takeoff_height"), 30.0);
	const double RthAltitude = ReadNumber(InData, TEXT("rth_altitude"), 100.0);
	const double MaxSpeed = ReadNumber(InData, TEXT("max_speed"));

	if (TargetLongitude == 0.0 || TargetLatitude == 0.0 || TargetHeight <= 0.0)
	{
		return InvalidParams;
	}

	CurrentFlightId = FlightId;
	CurrentRthAltitude = RthAltitude;
	if (MaxSpeed > 0.0)
	{
		// 指令指定最大速度时覆盖组件默认水平速度（v1 简化：任务期间生效）
		DroneSim->MaxHorizontalSpeed = MaxSpeed;
	}

	// 起飞到点：先爬升到安全起飞高度，再平飞至目标点
	const FUAVGeoCoordinate CurrentGeo = DroneSim->GetCurrentGeoCoordinate();
	TArray<FUAVWaypoint> Waypoints;
	FUAVWaypoint Climb;
	Climb.Latitude = CurrentGeo.Latitude;
	Climb.Longitude = CurrentGeo.Longitude;
	Climb.Altitude = SecurityTakeoffHeight;
	Climb.ArrivalThresholdMeters = DroneSim->DefaultArrivalThresholdMeters;
	FUAVWaypoint Target;
	Target.Latitude = TargetLatitude;
	Target.Longitude = TargetLongitude;
	Target.Altitude = TargetHeight;
	Target.ArrivalThresholdMeters = DroneSim->DefaultArrivalThresholdMeters;
	Waypoints.Add(Climb);
	Waypoints.Add(Target);

	TransitionTo(EUAVFlightState::TakingOff);
	DroneSim->SetWaypoints(Waypoints, true);
	OnTakeoffProgress.Broadcast(TEXT("task_ready"), FlightId, 0, DroneSim->GetRemainingDistance());
	UE_LOG(LogTemp, Log, TEXT("[UAVFlightControl] 起飞到点：flight_id=%s 目标(%.6f, %.6f) 高度=%.1fm"), *FlightId, TargetLatitude, TargetLongitude, TargetHeight);
	return Success;
}

int32 UUAVFlightControlComponent::HandleFlighttaskCreate(const TSharedPtr<FJsonObject>& InData)
{
	using namespace UAV::FlightControlResult;
	if (!bHasFlightAuthority) return NoAuthority;
	const FString FlightId = ReadString(InData, TEXT("flight_id"));
	if (FlightId.IsEmpty()) return InvalidParams;

	FUAVMissionEntry& Entry = Missions.FindOrAdd(FlightId);
	Entry.FlightId = FlightId;
	Entry.bPrepared = false;
	Entry.Waypoints.Reset();
	UE_LOG(LogTemp, Log, TEXT("[UAVFlightControl] 航线任务登记：flight_id=%s"), *FlightId);
	return Success;
}

int32 UUAVFlightControlComponent::HandleFlighttaskPrepare(const TSharedPtr<FJsonObject>& InData)
{
	using namespace UAV::FlightControlResult;
	if (!bHasFlightAuthority) return NoAuthority;
	const FString FlightId = ReadString(InData, TEXT("flight_id"));
	if (FlightId.IsEmpty()) return InvalidParams;

	FUAVMissionEntry& Entry = Missions.FindOrAdd(FlightId);
	Entry.FlightId = FlightId;
	Entry.bPrepared = true;
	Entry.RthAltitude = ReadNumber(InData, TEXT("rth_altitude"), 100.0);
	CurrentRthAltitude = Entry.RthAltitude;
	UE_LOG(LogTemp, Log, TEXT("[UAVFlightControl] 航线任务已就绪：flight_id=%s"), *FlightId);
	OnFlighttaskReady.Broadcast(FlightId);
	return Success;
}

int32 UUAVFlightControlComponent::HandleFlighttaskExecute(const TSharedPtr<FJsonObject>& InData)
{
	using namespace UAV::FlightControlResult;
	if (!DroneSim) return InternalError;
	if (!bHasFlightAuthority) return NoAuthority;
	const FString FlightId = ReadString(InData, TEXT("flight_id"));
	FUAVMissionEntry* Entry = FindMission(FlightId);
	if (!Entry) return UnknownFlightId;
	if (!Entry->bPrepared) return NotPrepared;

	// 中断当前任务并切换到新航线
	DroneSim->StopMission();
	CurrentFlightId = FlightId;

	TArray<FUAVWaypoint> Waypoints = Entry->Waypoints;
	if (Waypoints.Num() == 0)
	{
		// 未注入航线文件时使用默认演示航线（以当前位置为中心 100 米方形）
		const FUAVGeoCoordinate CurrentGeo = DroneSim->GetCurrentGeoCoordinate();
		BuildDefaultWayline(Waypoints, CurrentGeo, Entry->RthAltitude);
		UE_LOG(LogTemp, Warning, TEXT("[UAVFlightControl] flight_id=%s 无注入航点，使用默认演示航线"), *FlightId);
	}

	TransitionTo(EUAVFlightState::Wayline);
	DroneSim->SetWaypoints(Waypoints, true);
	OnFlighttaskProgress.Broadcast(TEXT("in_progress"), FlightId, 0, 0);
	UE_LOG(LogTemp, Log, TEXT("[UAVFlightControl] 航线任务开始：flight_id=%s 航点数=%d"), *FlightId, Waypoints.Num());
	return Success;
}

int32 UUAVFlightControlComponent::HandleFlighttaskUndo(const TSharedPtr<FJsonObject>& InData)
{
	using namespace UAV::FlightControlResult;
	if (!DroneSim) return InternalError;
	if (!bHasFlightAuthority) return NoAuthority;
	const FString FlightId = ReadString(InData, TEXT("flight_id"));
	if (!FlightId.IsEmpty() && FlightId != CurrentFlightId) return UnknownFlightId;

	if (FlightState == EUAVFlightState::Wayline || FlightState == EUAVFlightState::TakingOff)
	{
		DroneSim->StopMission();
		TransitionTo(EUAVFlightState::Flying);
		OnFlighttaskProgress.Broadcast(TEXT("canceled"), CurrentFlightId, 0, 0);
	}
	UE_LOG(LogTemp, Log, TEXT("[UAVFlightControl] 航线任务撤销：flight_id=%s"), *CurrentFlightId);
	return Success;
}

int32 UUAVFlightControlComponent::HandleFlighttaskPause(const TSharedPtr<FJsonObject>& InData)
{
	using namespace UAV::FlightControlResult;
	if (!DroneSim) return InternalError;
	if (!bHasFlightAuthority) return NoAuthority;
	if (FlightState != EUAVFlightState::Wayline) return StateConflict;

	DroneSim->PauseMission();
	OnFlighttaskProgress.Broadcast(TEXT("paused"), CurrentFlightId, DroneSim->GetCurrentWaypointIndex(), 0);
	UE_LOG(LogTemp, Log, TEXT("[UAVFlightControl] 航线任务暂停"));
	return Success;
}

int32 UUAVFlightControlComponent::HandleFlighttaskRecovery(const TSharedPtr<FJsonObject>& InData)
{
	using namespace UAV::FlightControlResult;
	if (!DroneSim) return InternalError;
	if (!bHasFlightAuthority) return NoAuthority;
	if (FlightState != EUAVFlightState::Wayline) return StateConflict;

	DroneSim->ResumeMission();
	OnFlighttaskProgress.Broadcast(TEXT("in_progress"), CurrentFlightId, DroneSim->GetCurrentWaypointIndex(), 0);
	UE_LOG(LogTemp, Log, TEXT("[UAVFlightControl] 航线任务恢复"));
	return Success;
}

int32 UUAVFlightControlComponent::HandleReturnHome(const TSharedPtr<FJsonObject>& InData)
{
	using namespace UAV::FlightControlResult;
	if (!bHasFlightAuthority) return NoAuthority;
	return StartReturnHome();
}

int32 UUAVFlightControlComponent::HandleReturnHomeCancel(const TSharedPtr<FJsonObject>& InData)
{
	using namespace UAV::FlightControlResult;
	if (!DroneSim) return InternalError;
	if (!bHasFlightAuthority) return NoAuthority;
	if (FlightState != EUAVFlightState::ReturnHome && FlightState != EUAVFlightState::Landing) return NotInReturnHome;

	DroneSim->StopMission();
	TransitionTo(EUAVFlightState::Flying);
	UE_LOG(LogTemp, Log, TEXT("[UAVFlightControl] 返航取消，当前悬停"));
	return Success;
}

int32 UUAVFlightControlComponent::HandleFlyToPoint(const TSharedPtr<FJsonObject>& InData)
{
	using namespace UAV::FlightControlResult;
	if (!DroneSim) return InternalError;
	if (!bHasFlightAuthority) return NoAuthority;
	if (FlightState == EUAVFlightState::TakingOff || FlightState == EUAVFlightState::Wayline
		|| FlightState == EUAVFlightState::ReturnHome || FlightState == EUAVFlightState::Landing)
	{
		return StateConflict;
	}

	const FString FlyToId = ReadString(InData, TEXT("fly_to_id"));
	const double MaxSpeed = ReadNumber(InData, TEXT("max_speed"));
	if (FlyToId.IsEmpty()) return InvalidParams;
	if (MaxSpeed < 1.0 || MaxSpeed > 15.0) return InvalidParams;

	FUAVWaypoint Target;
	if (!ParseFlyToTarget(InData, Target)) return InvalidParams;

	// 中断当前任务并飞向目标点（指点飞行复用空中巡航状态）
	DroneSim->StopMission();
	DroneSim->MaxHorizontalSpeed = MaxSpeed;
	CurrentFlyToId = FlyToId;
	bFlyToActive = true;
	TransitionTo(EUAVFlightState::Flying);
	DroneSim->SetWaypoints({ Target }, true);
	OnFlyToPointProgress.Broadcast(TEXT("wayline_progress"), FlyToId, 0, Success);
	UE_LOG(LogTemp, Log, TEXT("[UAVFlightControl] 指点飞行开始：fly_to_id=%s 目标(%.6f, %.6f) 高度=%.1fm 速度=%.1fm/s"), *FlyToId, Target.Latitude, Target.Longitude, Target.Altitude, MaxSpeed);
	return Success;
}

int32 UUAVFlightControlComponent::HandleFlyToPointStop(const TSharedPtr<FJsonObject>& InData)
{
	using namespace UAV::FlightControlResult;
	if (!DroneSim) return InternalError;
	if (!bHasFlightAuthority) return NoAuthority;
	if (!bFlyToActive) return StateConflict;

	DroneSim->StopMission();
	bFlyToActive = false;
	TransitionTo(EUAVFlightState::Flying);
	OnFlyToPointProgress.Broadcast(TEXT("wayline_cancel"), CurrentFlyToId, 0, Success);
	UE_LOG(LogTemp, Log, TEXT("[UAVFlightControl] 指点飞行停止：fly_to_id=%s"), *CurrentFlyToId);
	return Success;
}

int32 UUAVFlightControlComponent::HandleFlyToPointUpdate(const TSharedPtr<FJsonObject>& InData)
{
	using namespace UAV::FlightControlResult;
	if (!DroneSim) return InternalError;
	if (!bHasFlightAuthority) return NoAuthority;
	if (!bFlyToActive || FlightState != EUAVFlightState::Flying) return StateConflict;

	const double MaxSpeed = ReadNumber(InData, TEXT("max_speed"));
	if (MaxSpeed < 1.0 || MaxSpeed > 15.0) return InvalidParams;

	FUAVWaypoint Target;
	if (!ParseFlyToTarget(InData, Target)) return InvalidParams;

	// 复用当前指点飞行会话：更新目标点与速度
	DroneSim->MaxHorizontalSpeed = MaxSpeed;
	DroneSim->SetWaypoints({ Target }, true);
	OnFlyToPointProgress.Broadcast(TEXT("wayline_progress"), CurrentFlyToId, 0, Success);
	UE_LOG(LogTemp, Log, TEXT("[UAVFlightControl] 指点飞行更新：fly_to_id=%s 新目标(%.6f, %.6f) 高度=%.1fm 速度=%.1fm/s"), *CurrentFlyToId, Target.Latitude, Target.Longitude, Target.Altitude, MaxSpeed);
	return Success;
}

int32 UUAVFlightControlComponent::HandleDrcModeEnter(const TSharedPtr<FJsonObject>& InData)
{
	using namespace UAV::FlightControlResult;
	if (!bHasFlightAuthority) return NoAuthority;
	// DRC 直控要求无人机已离地（待机状态禁止进入）
	if (FlightState == EUAVFlightState::Idle) return StateConflict;

	bDrcActive = true;
	OnDrcStatusNotify.Broadcast(2);
	UE_LOG(LogTemp, Log, TEXT("[UAVFlightControl] 进入 DRC 会话"));
	return Success;
}

int32 UUAVFlightControlComponent::HandleDrcModeExit(const TSharedPtr<FJsonObject>& InData)
{
	using namespace UAV::FlightControlResult;
	if (!bHasFlightAuthority) return NoAuthority;
	if (!bDrcActive) return StateConflict;

	// 退出会话：停止摇杆控制并复位会话状态
	bDrcActive = false;
	if (bJoystickControlActive && DroneSim)
	{
		DroneSim->SetJoystickActive(false);
	}
	bJoystickControlActive = false;
	OnDrcStatusNotify.Broadcast(0);
	UE_LOG(LogTemp, Log, TEXT("[UAVFlightControl] 退出 DRC 会话"));
	return Success;
}

int32 UUAVFlightControlComponent::HandleDroneControl(const TSharedPtr<FJsonObject>& InData)
{
	using namespace UAV::FlightControlResult;
	if (!DroneSim) return InternalError;
	if (!bDrcActive) return StateConflict;

	// 参数范围对齐 dock 校验口径：x/y ∈ [-17,17]、h ∈ [-4,5]、w ∈ [-90,90]、freq ∈ [2,10]、delayTime ∈ [100,1000]
	const int32 Seq = static_cast<int32>(ReadNumber(InData, TEXT("seq"), -1.0));
	const int32 X = static_cast<int32>(ReadNumber(InData, TEXT("x"), 0.0));
	const int32 Y = static_cast<int32>(ReadNumber(InData, TEXT("y"), 0.0));
	const int32 H = static_cast<int32>(ReadNumber(InData, TEXT("h"), 0.0));
	const int32 W = static_cast<int32>(ReadNumber(InData, TEXT("w"), 0.0));
	const int32 Freq = static_cast<int32>(ReadNumber(InData, TEXT("freq"), 0.0));
	const int32 DelayTime = static_cast<int32>(ReadNumber(InData, TEXT("delayTime"), 0.0));

	if (Seq < 0 || X < -17 || X > 17 || Y < -17 || Y > 17 || H < -4 || H > 5
		|| W < -90 || W > 90 || Freq < 2 || Freq > 10 || DelayTime < 100 || DelayTime > 1000)
	{
		return InvalidParams;
	}

	// 停止现有任务并驱动摇杆速度（DRC 直控与任务互斥）
	DroneSim->StopMission();
	DroneSim->SetJoystickCommand(X, Y, H, W, Freq, DelayTime);
	bJoystickControlActive = true;
	LastDrcSeq = Seq;
	UE_LOG(LogTemp, Log, TEXT("[UAVFlightControl] 摇杆控制：seq=%d x=%d y=%d h=%d w=%d freq=%d delayTime=%d"), Seq, X, Y, H, W, Freq, DelayTime);
	return Success;
}

int32 UUAVFlightControlComponent::HandleHeartBeat(const TSharedPtr<FJsonObject>& InData)
{
	using namespace UAV::FlightControlResult;
	if (!bDrcActive) return StateConflict;

	const int32 Seq = static_cast<int32>(ReadNumber(InData, TEXT("seq"), -1.0));
	if (Seq < 0) return InvalidParams;
	LastDrcSeq = Seq;
	UE_LOG(LogTemp, Log, TEXT("[UAVFlightControl] DRC 心跳：seq=%d"), Seq);
	return Success;
}

int32 UUAVFlightControlComponent::HandleDroneEmergencyStop(const TSharedPtr<FJsonObject>& InData)
{
	using namespace UAV::FlightControlResult;
	if (!DroneSim) return InternalError;

	// 急停：无条件停止一切运动与任务，保持 DRC 会话（对齐真实设备行为）
	DroneSim->SetJoystickActive(false);
	DroneSim->StopMission();
	bJoystickControlActive = false;
	UE_LOG(LogTemp, Log, TEXT("[UAVFlightControl] DRC 急停：停止全部运动"));
	return Success;
}

void UUAVFlightControlComponent::TransitionTo(EUAVFlightState InNewState)
{
	FlightState = InNewState;
	if (DroneSim)
	{
		DroneSim->SetFlightState(InNewState);
	}
}

int32 UUAVFlightControlComponent::StartReturnHome()
{
	using namespace UAV::FlightControlResult;
	if (!DroneSim) return InternalError;
	// 返航/降落中不重复触发
	if (FlightState == EUAVFlightState::ReturnHome || FlightState == EUAVFlightState::Landing)
	{
		return StateConflict;
	}

	// 中断当前任务，飞回机场（返航高度）
	DroneSim->StopMission();
	TArray<FUAVWaypoint> Waypoints;
	FUAVWaypoint Home;
	Home.Latitude = DroneSim->AirportOrigin.Latitude;
	Home.Longitude = DroneSim->AirportOrigin.Longitude;
	Home.Altitude = CurrentRthAltitude;
	Home.ArrivalThresholdMeters = DroneSim->DefaultArrivalThresholdMeters;
	Waypoints.Add(Home);

	TransitionTo(EUAVFlightState::ReturnHome);
	DroneSim->SetWaypoints(Waypoints, true);
	UE_LOG(LogTemp, Log, TEXT("[UAVFlightControl] 返航开始：返航高度=%.1fm"), CurrentRthAltitude);
	return Success;
}

void UUAVFlightControlComponent::BuildDefaultWayline(TArray<FUAVWaypoint>& OutWaypoints, const FUAVGeoCoordinate& InCenter, double InAltitude) const
{
	const double HalfSize = 50.0; // 半边长（米）

	// 方形航线：东北 → 东南 → 西南 → 西北
	const TArray<FVector2D> Offsets = {
		FVector2D( HalfSize,  HalfSize),
		FVector2D( HalfSize, -HalfSize),
		FVector2D(-HalfSize, -HalfSize),
		FVector2D(-HalfSize,  HalfSize),
	};
	for (const FVector2D& Offset : Offsets)
	{
		FUAVWaypoint Waypoint;
		Waypoint.Longitude = InCenter.Longitude + UUAVGeoUtils::EastMetersToLonDelta(InCenter.Latitude, Offset.X);
		Waypoint.Latitude = InCenter.Latitude + UUAVGeoUtils::NorthMetersToLatDelta(Offset.Y);
		Waypoint.Altitude = InAltitude;
		Waypoint.ArrivalThresholdMeters = 1.5;
		OutWaypoints.Add(Waypoint);
	}
}

FUAVMissionEntry* UUAVFlightControlComponent::FindMission(const FString& InFlightId)
{
	return Missions.Find(InFlightId);
}

void UUAVFlightControlComponent::OnDroneWaypointReached(int32 WaypointIndex)
{
	if (FlightState == EUAVFlightState::TakingOff)
	{
		OnTakeoffProgress.Broadcast(TEXT("wayline_progress"), CurrentFlightId, WaypointIndex, DroneSim ? DroneSim->GetRemainingDistance() : 0.0);
	}
	else if (FlightState == EUAVFlightState::Wayline && DroneSim)
	{
		const int32 Total = DroneSim->GetWaypointCount();
		const int32 Percent = Total > 0 ? (WaypointIndex + 1) * 100 / Total : 0;
		OnFlighttaskProgress.Broadcast(TEXT("in_progress"), CurrentFlightId, WaypointIndex, Percent);
	}
	else if (bFlyToActive)
	{
		// 指点飞行到达目标点（单点航线）：广播进度（航点索引与无人机模拟组件一致，0 基）
		OnFlyToPointProgress.Broadcast(TEXT("wayline_progress"), CurrentFlyToId, WaypointIndex, UAV::FlightControlResult::Success);
	}
}

void UUAVFlightControlComponent::OnDroneMissionFinished()
{
	if (FlightState == EUAVFlightState::TakingOff)
	{
		// 起飞到点完成：到达目标点悬停
		OnTakeoffProgress.Broadcast(TEXT("wayline_ok"), CurrentFlightId, 0, 0.0);
		OnTakeoffProgress.Broadcast(TEXT("task_finish"), CurrentFlightId, 0, 0.0);
		TransitionTo(EUAVFlightState::Flying);
	}
	else if (FlightState == EUAVFlightState::Wayline)
	{
		OnFlighttaskProgress.Broadcast(TEXT("ok"), CurrentFlightId, 0, 100);
		TransitionTo(EUAVFlightState::Flying);
	}
	else if (bFlyToActive)
	{
		// 指点飞行完成：到达目标点后悬停，保持空中巡航状态
		OnFlyToPointProgress.Broadcast(TEXT("wayline_ok"), CurrentFlyToId, 0, UAV::FlightControlResult::Success);
		bFlyToActive = false;
		UE_LOG(LogTemp, Log, TEXT("[UAVFlightControl] 指点飞行完成：fly_to_id=%s"), *CurrentFlyToId);
	}
	else if (FlightState == EUAVFlightState::ReturnHome)
	{
		// 到达返航点后开始降落至机场地面
		TransitionTo(EUAVFlightState::Landing);
		if (DroneSim)
		{
			TArray<FUAVWaypoint> Waypoints;
			FUAVWaypoint Ground;
			Ground.Latitude = DroneSim->AirportOrigin.Latitude;
			Ground.Longitude = DroneSim->AirportOrigin.Longitude;
			Ground.Altitude = DroneSim->AirportOrigin.Altitude;
			Ground.ArrivalThresholdMeters = DroneSim->DefaultArrivalThresholdMeters;
			Waypoints.Add(Ground);
			DroneSim->SetWaypoints(Waypoints, true);
		}
	}
	else if (FlightState == EUAVFlightState::Landing)
	{
		bReturnHomePending = false;
		TransitionTo(EUAVFlightState::Idle);
	}
}

void UUAVFlightControlComponent::OnDroneBatteryLow(double CapacityPercent)
{
	// 仅持有飞控权、处于空中状态（起飞/航线/巡航）且未在返航流程中时触发
	if (!bHasFlightAuthority || bReturnHomePending)
	{
		return;
	}
	if (FlightState != EUAVFlightState::TakingOff
		&& FlightState != EUAVFlightState::Wayline
		&& FlightState != EUAVFlightState::Flying)
	{
		return;
	}

	const int32 Result = StartReturnHome();
	if (Result == UAV::FlightControlResult::Success)
	{
		bReturnHomePending = true;
		OnReturnHomeStatus.Broadcast(TEXT("rth_auto_trigger"), TEXT("battery_low"));
		UE_LOG(LogTemp, Warning, TEXT("[UAVFlightControl] 低电量自动返航：当前电量=%.1f%%"), CapacityPercent);
	}
}
