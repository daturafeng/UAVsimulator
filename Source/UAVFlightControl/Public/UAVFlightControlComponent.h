// 飞控指令状态机：解析上云 API services 指令并驱动无人机运动模拟
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Dom/JsonObject.h"
#include "UAVDroneSimComponent.h"
#include "UAVFlightControlComponent.generated.h"

/** 指令处理结果码（0=成功；与上云 API result 语义一致） */
namespace UAV::FlightControlResult
{
	constexpr int32 Success = 0;
	constexpr int32 InternalError = 1;
	constexpr int32 NoAuthority = 2;
	constexpr int32 StateConflict = 3;
	constexpr int32 UnknownFlightId = 4;
	constexpr int32 NotPrepared = 5;
	constexpr int32 UnknownMethod = 6;
	constexpr int32 InvalidParams = 7;
	constexpr int32 NotInReturnHome = 8;
}

/** 指令处理结果事件（Method + result 码） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUAVCommandResultDelegate, const FString&, Method, int32, Result);

/** 起飞到点进度事件（Status: task_ready / wayline_progress / wayline_ok / task_finish） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FUAVTakeoffProgressDelegate, const FString&, Status, const FString&, FlightId, int32, WayPointIndex, double, RemainingDistance);

/** 航线任务进度事件（Status: sent / in_progress / ok / paused / rejected / failed / canceled / timeout / partially_done） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FUAVFlighttaskProgressDelegate, const FString&, Status, const FString&, FlightId, int32, CurrentWaypointIndex, int32, Percent);

/** 返航状态事件（Status: rth_auto_trigger 等；Reason: battery_low 等） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUAVReturnHomeStatusDelegate, const FString&, Status, const FString&, Reason);

/** 任务就绪事件（flighttask_prepare 成功后广播，携带 flight_id） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUAVFlighttaskReadyDelegate, const FString&, FlightId);

/** 指点飞行进度事件（Status: wayline_progress / wayline_ok / wayline_cancel / wayline_failed，对齐 dock FlyToStatusEnum；Result: 0=成功） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FUAVFlyToPointProgressDelegate, const FString&, Status, const FString&, FlyToId, int32, WayPointIndex, int32, Result);

/** DRC 会话状态事件（参数：drc_state，对齐 dock DrcStateEnum：0=断开、1=连接中、2=已连接） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUAVDrcStatusNotifyDelegate, int32, DrcState);

/** 航线任务条目：flighttask_create/prepare 阶段登记，execute 时消费 */
struct FUAVMissionEntry
{
	/** flight_id */
	FString FlightId;

	/** 是否已 prepare */
	bool bPrepared = false;

	/** 返航高度（米） */
	double RthAltitude = 0.0;

	/** 航线航点（由 SetWaylineWaypoints 注入，模拟 KMZ 航线文件） */
	TArray<FUAVWaypoint> Waypoints;
};

/**
 * 飞控指令状态机：解析上云 API services 指令（flight_authority_grab / takeoff_to_point /
 * flighttask_* / return_home_*），校验飞控权并驱动 UAVDroneSim 运动，产出事件供桥接层转发。
 * live_* 直播指令由 UAVCameraStream 组件处理。
 */
UCLASS(ClassGroup = (UAV), meta = (BlueprintSpawnableComponent))
class UAVFLIGHTCONTROL_API UUAVFlightControlComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUAVFlightControlComponent();

	/** 注入无人机模拟组件（必须在 BeginPlay 前调用） */
	UFUNCTION(BlueprintCallable, Category = "UAV|FlightControl")
	void SetDroneSim(UUAVDroneSimComponent* InDroneSim);

	/**
	 * 处理一条上云 API services 指令。
	 * InMethod 为 method（如 takeoff_to_point），InDataJson 为指令 data 字段的 JSON 字符串（可为空）。
	 * 返回 result 码（0=成功）；未知 method 返回 UnknownMethod。
	 */
	UFUNCTION(BlueprintCallable, Category = "UAV|FlightControl")
	int32 HandleCommand(const FString& InMethod, const FString& InDataJson);

	/** 是否已抢占飞行权 */
	UFUNCTION(BlueprintPure, Category = "UAV|FlightControl")
	bool HasFlightAuthority() const { return bHasFlightAuthority; }

	/** 当前飞控状态 */
	UFUNCTION(BlueprintPure, Category = "UAV|FlightControl")
	EUAVFlightState GetFlightState() const { return FlightState; }

	/** 为指定 flightId 注入航点（模拟 KMZ 航线文件；execute 时若无航点则使用默认演示航线） */
	UFUNCTION(BlueprintCallable, Category = "UAV|FlightControl")
	void SetWaylineWaypoints(const FString& InFlightId, const TArray<FUAVWaypoint>& InWaypoints);

	/** 当前任务 flight_id（可能为空串） */
	UFUNCTION(BlueprintPure, Category = "UAV|FlightControl")
	FString GetCurrentFlightId() const { return CurrentFlightId; }

	/** 当前返航高度（米） */
	UFUNCTION(BlueprintPure, Category = "UAV|FlightControl")
	double GetCurrentRthAltitude() const { return CurrentRthAltitude; }

	/** 最近一次 DRC 指令（drone_control / heart_beat）携带的 seq；未收到时返回 -1 */
	UFUNCTION(BlueprintPure, Category = "UAV|FlightControl")
	int32 GetLastDrcSeq() const { return LastDrcSeq; }

	/** 指令处理结果事件 */
	UPROPERTY(BlueprintAssignable, Category = "UAV|FlightControl|Event")
	FUAVCommandResultDelegate OnCommandResult;

	/** 起飞到点进度事件 */
	UPROPERTY(BlueprintAssignable, Category = "UAV|FlightControl|Event")
	FUAVTakeoffProgressDelegate OnTakeoffProgress;

	/** 航线任务进度事件 */
	UPROPERTY(BlueprintAssignable, Category = "UAV|FlightControl|Event")
	FUAVFlighttaskProgressDelegate OnFlighttaskProgress;

	/** 返航状态事件（自动返航触发/人工返航状态） */
	UPROPERTY(BlueprintAssignable, Category = "UAV|FlightControl|Event")
	FUAVReturnHomeStatusDelegate OnReturnHomeStatus;

	/** 任务就绪事件（flighttask_prepare 成功后广播） */
	UPROPERTY(BlueprintAssignable, Category = "UAV|FlightControl|Event")
	FUAVFlighttaskReadyDelegate OnFlighttaskReady;

	/** 指点飞行进度事件 */
	UPROPERTY(BlueprintAssignable, Category = "UAV|FlightControl|Event")
	FUAVFlyToPointProgressDelegate OnFlyToPointProgress;

	/** DRC 会话状态事件（进入/退出时广播） */
	UPROPERTY(BlueprintAssignable, Category = "UAV|FlightControl|Event")
	FUAVDrcStatusNotifyDelegate OnDrcStatusNotify;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---- 指令处理（按 method 分发） ----
	int32 HandleAuthorityGrab(const TSharedPtr<FJsonObject>& InData);
	int32 HandleTakeoffToPoint(const TSharedPtr<FJsonObject>& InData);
	int32 HandleFlighttaskCreate(const TSharedPtr<FJsonObject>& InData);
	int32 HandleFlighttaskPrepare(const TSharedPtr<FJsonObject>& InData);
	int32 HandleFlighttaskExecute(const TSharedPtr<FJsonObject>& InData);
	int32 HandleFlighttaskUndo(const TSharedPtr<FJsonObject>& InData);
	int32 HandleFlighttaskPause(const TSharedPtr<FJsonObject>& InData);
	int32 HandleFlighttaskRecovery(const TSharedPtr<FJsonObject>& InData);
	int32 HandleReturnHome(const TSharedPtr<FJsonObject>& InData);
	int32 HandleReturnHomeCancel(const TSharedPtr<FJsonObject>& InData);
	int32 HandleFlyToPoint(const TSharedPtr<FJsonObject>& InData);
	int32 HandleFlyToPointStop(const TSharedPtr<FJsonObject>& InData);
	int32 HandleFlyToPointUpdate(const TSharedPtr<FJsonObject>& InData);
	int32 HandleDrcModeEnter(const TSharedPtr<FJsonObject>& InData);
	int32 HandleDrcModeExit(const TSharedPtr<FJsonObject>& InData);
	int32 HandleDroneControl(const TSharedPtr<FJsonObject>& InData);
	int32 HandleHeartBeat(const TSharedPtr<FJsonObject>& InData);
	int32 HandleDroneEmergencyStop(const TSharedPtr<FJsonObject>& InData);

	// ---- 状态机辅助 ----
	/** 状态迁移：同步飞控状态与无人机模拟组件 */
	void TransitionTo(EUAVFlightState InNewState);

	/** 启动返航：中断当前任务并飞回机场（返航高度）；状态冲突返回 StateConflict */
	int32 StartReturnHome();

	/** 以当前位置为中心生成 100 米方形演示航线（未注入航线文件时的兜底） */
	void BuildDefaultWayline(TArray<FUAVWaypoint>& OutWaypoints, const FUAVGeoCoordinate& InCenter, double InAltitude) const;

	/** 查找已登记的航线任务 */
	FUAVMissionEntry* FindMission(const FString& InFlightId);

	// ---- 无人机事件回调（BeginPlay 时绑定） ----
	UFUNCTION()
	void OnDroneWaypointReached(int32 WaypointIndex);

	UFUNCTION()
	void OnDroneMissionFinished();

	/** 低电量事件回调：满足条件时自动返航并广播 OnReturnHomeStatus */
	UFUNCTION()
	void OnDroneBatteryLow(double CapacityPercent);

	/** 关联的无人机模拟组件 */
	UPROPERTY()
	TObjectPtr<UUAVDroneSimComponent> DroneSim;

private:
	/** 是否已抢占飞行权 */
	bool bHasFlightAuthority = false;

	/** 当前飞控状态（状态机：Idle/TakingOff/Wayline/ReturnHome/Landing/Flying） */
	EUAVFlightState FlightState = EUAVFlightState::Idle;

	/** 当前任务 flight_id */
	FString CurrentFlightId;

	/** 返航高度（米，由 takeoff_to_point rth_altitude / flighttask_prepare 设置） */
	double CurrentRthAltitude = 100.0;

	/** 已登记的航线任务（flighttask_create/prepare） */
	TMap<FString, FUAVMissionEntry> Missions;

	/** 自动返航已触发（防抖：返航/降落期间不重复触发） */
	bool bReturnHomePending = false;

	/** 指点飞行会话活动标记（当前是否处于指点飞行） */
	bool bFlyToActive = false;

	/** 当前指点飞行 fly_to_id */
	FString CurrentFlyToId;

	/** DRC 会话激活（drc_mode_enter 后为 true，exit 后为 false） */
	bool bDrcActive = false;

	/** 摇杆直控激活（drone_control 成功后为 true，急停/退出 DRC 后为 false） */
	bool bJoystickControlActive = false;

	/** 最近 DRC 指令 seq（drone_control / heart_beat；用于 drc/up 回执 output.seq） */
	int32 LastDrcSeq = -1;
};
