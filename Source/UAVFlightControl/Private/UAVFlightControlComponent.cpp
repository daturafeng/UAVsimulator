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
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[UAVFlightControl] DroneSim 未注入，飞行指令将无法执行"));
	}
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
	if (!DroneSim) return InternalError;
	if (!bHasFlightAuthority) return NoAuthority;
	if (FlightState == EUAVFlightState::ReturnHome || FlightState == EUAVFlightState::Landing) return StateConflict;

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

void UUAVFlightControlComponent::TransitionTo(EUAVFlightState InNewState)
{
	FlightState = InNewState;
	if (DroneSim)
	{
		DroneSim->SetFlightState(InNewState);
	}
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
		TransitionTo(EUAVFlightState::Idle);
	}
}
