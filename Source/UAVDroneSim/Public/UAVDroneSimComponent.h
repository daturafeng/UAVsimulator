// 无人机本体模拟组件：航点队列 + 速度可控移动 + 遥测查询
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UAVGeoUtils.h"
#include "UAVDroneSimComponent.generated.h"

/** 无人机飞行状态（与上云 API mode_code 的映射由 OSD 层负责） */
UENUM(BlueprintType)
enum class EUAVFlightState : uint8
{
	/** 待机（停机坪） */
	Idle,
	/** 起飞/爬升 */
	TakingOff,
	/** 航线任务执行中 */
	Wayline,
	/** 空中巡航（起飞到点/自由移动） */
	Flying,
	/** 降落 */
	Landing,
	/** 返航 */
	ReturnHome
};

/** 航点：经纬度/海拔 + 到达阈值（米） */
USTRUCT(BlueprintType)
struct FUAVWaypoint
{
	GENERATED_BODY()

	/** 纬度（度，北纬为正） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Waypoint")
	double Latitude = 0.0;

	/** 经度（度，东经为正） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Waypoint")
	double Longitude = 0.0;

	/** 海拔（米，相对海平面） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Waypoint")
	double Altitude = 0.0;

	/** 到达判定阈值（米）；<=0 时使用组件默认阈值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Waypoint")
	double ArrivalThresholdMeters = 1.5;
};

/** 到达某航点时广播（参数：航点索引，0 起） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUAVWaypointReachedDelegate, int32, WaypointIndex);

/** 全部航点执行完毕（或直接移动到达）时广播 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FUAVMissionFinishedDelegate);

/**
 * 无人机模拟组件：维护航点队列、位置、朝向与速度，
 * 由飞控指令（UAVFlightControl）驱动，按"航点 + 速度矢量"模型平滑移动。
 */
UCLASS(ClassGroup = (UAV), meta = (BlueprintSpawnableComponent))
class UAVDRONESIM_API UUAVDroneSimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUAVDroneSimComponent();

	// ---- 配置 ----
	/** 机场原点（经纬度/海拔），局部 ENU 坐标原点（X=东、Y=北、Z=上） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Config")
	FUAVGeoCoordinate AirportOrigin;

	/** 最大水平速度（米/秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Config", meta = (ClampMin = "0.1"))
	double MaxHorizontalSpeed = 10.0;

	/** 最大垂直速度（米/秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Config", meta = (ClampMin = "0.1"))
	double MaxVerticalSpeed = 3.0;

	/** 默认到达阈值（米），航点未指定时使用 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Config", meta = (ClampMin = "0.1"))
	double DefaultArrivalThresholdMeters = 1.5;

	// ---- 事件 ----
	/** 到达航点事件（参数：航点索引） */
	UPROPERTY(BlueprintAssignable, Category = "UAV|Event")
	FUAVWaypointReachedDelegate OnWaypointReached;

	/** 任务完成事件（全部航点执行完毕或直接移动到达） */
	UPROPERTY(BlueprintAssignable, Category = "UAV|Event")
	FUAVMissionFinishedDelegate OnMissionFinished;

	// ---- 遥测查询 ----
	/** 当前飞行状态 */
	UFUNCTION(BlueprintPure, Category = "UAV|Telemetry")
	EUAVFlightState GetFlightState() const { return FlightState; }

	/** 设置飞行状态（由飞控状态机调用） */
	UFUNCTION(BlueprintCallable, Category = "UAV|Telemetry")
	void SetFlightState(EUAVFlightState NewState) { FlightState = NewState; }

	/** 当前位置（场景坐标，米制：X=东、Y=北、Z=上） */
	UFUNCTION(BlueprintPure, Category = "UAV|Telemetry")
	FVector GetCurrentLocation() const { return CurrentLocation; }

	/** 当前位置（经纬度/海拔） */
	UFUNCTION(BlueprintPure, Category = "UAV|Telemetry")
	FUAVGeoCoordinate GetCurrentGeoCoordinate() const;

	/** 当前朝向（度，0=北、顺时针） */
	UFUNCTION(BlueprintPure, Category = "UAV|Telemetry")
	double GetHeadingDegrees() const { return HeadingDegrees; }

	/** 当前水平速度（米/秒） */
	UFUNCTION(BlueprintPure, Category = "UAV|Telemetry")
	double GetHorizontalSpeed() const { return CurrentHorizontalSpeed; }

	/** 当前垂直速度（米/秒，正=上升） */
	UFUNCTION(BlueprintPure, Category = "UAV|Telemetry")
	double GetVerticalSpeed() const { return CurrentVerticalSpeed; }

	/** 到当前目标（航点/直接移动目标）的距离（米） */
	UFUNCTION(BlueprintPure, Category = "UAV|Telemetry")
	double GetRemainingDistance() const;

	/** 剩余任务总距离（米）：剩余航点段距离之和 */
	UFUNCTION(BlueprintPure, Category = "UAV|Telemetry")
	double GetRemainingMissionDistance() const;

	/** 当前航点索引（0 起；无航线任务时返回 -1） */
	UFUNCTION(BlueprintPure, Category = "UAV|Telemetry")
	int32 GetCurrentWaypointIndex() const;

	/** 是否有进行中的任务（航线或直接移动） */
	UFUNCTION(BlueprintPure, Category = "UAV|Telemetry")
	bool HasActiveMission() const { return bMissionActive; }

	/** 航点数量 */
	UFUNCTION(BlueprintPure, Category = "UAV|Telemetry")
	int32 GetWaypointCount() const { return Waypoints.Num(); }

	// ---- 任务接口 ----
	/** 设置航点队列（替换现有）；bStartImmediately 为真时立即开始执行 */
	UFUNCTION(BlueprintCallable, Category = "UAV|Mission")
	bool SetWaypoints(const TArray<FUAVWaypoint>& InWaypoints, bool bStartImmediately = false);

	/** 追加一个航点（任务未开始时生效） */
	UFUNCTION(BlueprintCallable, Category = "UAV|Mission")
	void AddWaypoint(const FUAVWaypoint& InWaypoint);

	/** 清空航点队列并停止任务 */
	UFUNCTION(BlueprintCallable, Category = "UAV|Mission")
	void ClearWaypoints();

	/** 开始执行航线任务（从航点索引 0 开始） */
	UFUNCTION(BlueprintCallable, Category = "UAV|Mission")
	bool StartMission();

	/** 暂停任务（保持位置） */
	UFUNCTION(BlueprintCallable, Category = "UAV|Mission")
	void PauseMission();

	/** 恢复任务 */
	UFUNCTION(BlueprintCallable, Category = "UAV|Mission")
	void ResumeMission();

	/** 停止任务（保留航点供检查，飞行状态由飞控层决定） */
	UFUNCTION(BlueprintCallable, Category = "UAV|Mission")
	void StopMission();

	/** 直接移动到指定场景坐标（旧接口兼容：清空航点、无中间点） */
	UFUNCTION(BlueprintCallable, Category = "UAV|Mission")
	void MoveToLocation(const FVector& NewLocation);

	/** 每帧推进逻辑（由拥有者驱动） */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

	/** 计算朝向（度）：水平移动方向，0=北、顺时针；无水平移动时保持原朝向 */
	void UpdateHeading(const FVector& InHorizontalDir);

	/** 当前目标场景坐标（航点或直接移动目标）；无进行中任务时返回 false */
	bool GetCurrentTargetScene(FVector& OutTarget) const;

	/** 当前到达判定阈值（米） */
	double GetArrivalThreshold() const;

private:
	/** 航点队列 */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Mission", meta = (AllowPrivateAccess = "true"))
	TArray<FUAVWaypoint> Waypoints;

	/** 当前航点索引 */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Mission", meta = (AllowPrivateAccess = "true"))
	int32 CurrentWaypointIndex = 0;

	/** 任务进行中（航线或直接移动） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Mission", meta = (AllowPrivateAccess = "true"))
	bool bMissionActive = false;

	/** 任务暂停 */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Mission", meta = (AllowPrivateAccess = "true"))
	bool bMissionPaused = false;

	/** 直接移动模式（无航点，直接飞向目标点） */
	UPROPERTY()
	bool bDirectMove = false;

	/** 直接移动目标（场景坐标） */
	UPROPERTY()
	FVector TargetLocation = FVector::ZeroVector;

	/** 当前位置（场景坐标） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Telemetry", meta = (AllowPrivateAccess = "true"))
	FVector CurrentLocation = FVector::ZeroVector;

	/** 当前飞行状态 */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Telemetry", meta = (AllowPrivateAccess = "true"))
	EUAVFlightState FlightState = EUAVFlightState::Idle;

	/** 当前水平速度（米/秒） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Telemetry", meta = (AllowPrivateAccess = "true"))
	double CurrentHorizontalSpeed = 0.0;

	/** 当前垂直速度（米/秒，正=上升） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Telemetry", meta = (AllowPrivateAccess = "true"))
	double CurrentVerticalSpeed = 0.0;

	/** 当前朝向（度，0=北、顺时针） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Telemetry", meta = (AllowPrivateAccess = "true"))
	double HeadingDegrees = 0.0;
};
