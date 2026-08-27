// 无人机本体模拟组件：航点队列 + 速度可控移动 + 遥测查询
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UAVGeoUtils.h"
#include "UAVPayloadMath.h"
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

/** 电量首次低于返航阈值时广播（参数：当前电量百分比） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUAVBatteryLowDelegate, double, CapacityPercent);

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

	// ---- 载荷配置：电量 ----
	/** 初始电量百分比（0-100） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Payload", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	double BatteryCapacityStartPercent = 100.0;

	/** 飞行状态电量消耗速率（%/秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Payload", meta = (ClampMin = "0.0"))
	double BatteryDrainPercentPerSecond = 0.05;

	/** 待机状态电量消耗速率（%/秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Payload", meta = (ClampMin = "0.0"))
	double BatteryIdleDrainPercentPerSecond = 0.005;

	/** 降落电量阈值（%，OSD landing_power，对齐 dock=20） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Payload", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	double BatteryLandingPowerPercent = 20.0;

	/** 返航电量阈值（%，OSD return_home_power 与低电量事件阈值，对齐 dock=25） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Payload", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	double BatteryReturnHomePowerPercent = 25.0;

	// ---- 载荷配置：云台 ----
	/** 云台模拟参数（俯仰/横滚/偏航） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Payload")
	FUAVGimbalConfig GimbalConfig;

	// ---- 载荷配置：相机 ----
	/** 相机模式：0 拍照 / 1 录像（对齐 dock int 枚举） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Payload", meta = (ClampMin = "0", ClampMax = "1"))
	int32 CameraMode = 1;

	/** 变焦倍率下限 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Payload", meta = (ClampMin = "1.0"))
	double ZoomFactorMin = 1.0;

	/** 变焦倍率上限 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Payload", meta = (ClampMin = "1.0"))
	double ZoomFactorMax = 7.0;

	/** 初始变焦倍率（对齐 dock 3.0 基线） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Payload", meta = (ClampMin = "1.0"))
	double DefaultZoomFactor = 3.0;

	// ---- 事件 ----
	/** 到达航点事件（参数：航点索引） */
	UPROPERTY(BlueprintAssignable, Category = "UAV|Event")
	FUAVWaypointReachedDelegate OnWaypointReached;

	/** 任务完成事件（全部航点执行完毕或直接移动到达） */
	UPROPERTY(BlueprintAssignable, Category = "UAV|Event")
	FUAVMissionFinishedDelegate OnMissionFinished;

	/** 低电量事件（电量首次低于返航阈值时广播） */
	UPROPERTY(BlueprintAssignable, Category = "UAV|Payload|Event")
	FUAVBatteryLowDelegate OnBatteryLow;

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

	// ---- 载荷遥测 ----
	/** 当前电量百分比（0-100） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	double GetBatteryCapacityPercent() const { return BatteryCapacityPercent; }

	/** 降落电量阈值（%） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	double GetLandingPowerPercent() const { return BatteryLandingPowerPercent; }

	/** 返航电量阈值（%） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	double GetReturnHomePowerPercent() const { return BatteryReturnHomePowerPercent; }

	/** 剩余飞行时间（秒，按飞行消耗速率估算） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	double GetRemainFlightTimeSeconds() const;

	/** 电池单元温度/电压（index 0/1，对齐 dock 双电池口径） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	void GetBatteryCell(int32 InIndex, double& OutTemperatureCelsius, int32& OutVoltageMv) const;

	/** 云台角度状态 */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	FUAVGimbalState GetGimbalState() const { return GimbalState; }

	/** 云台俯仰角（度） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	double GetGimbalPitchDegrees() const { return GimbalState.PitchDegrees; }

	/** 云台横滚角（度） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	double GetGimbalRollDegrees() const { return GimbalState.RollDegrees; }

	/** 云台偏航角（度） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	double GetGimbalYawDegrees() const { return GimbalState.YawDegrees; }

	/** 相机模式（0 拍照 / 1 录像） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	int32 GetCameraMode() const { return CameraMode; }

	/** 设置相机模式（钳制到 0/1） */
	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void SetCameraMode(int32 NewMode);

	/** 当前变焦倍率 */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	double GetZoomFactor() const { return ZoomFactor; }

	/** 设置变焦倍率（钳制到配置范围） */
	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void SetZoomFactor(double NewZoomFactor);

	// ---- 相机设置状态（曝光/对焦/测光/存储/分屏/焦距/看点/POI） ----
	/** 曝光模式（0 自动 / 1 手动 / 2 快门优先 / 3 光圈优先，对齐上云 API exposure_mode） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	int32 GetExposureMode() const { return ExposureMode; }

	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void SetExposureMode(int32 NewMode);

	/** 快门速度（秒，如 1/1000） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	double GetShutterSpeed() const { return ShutterSpeed; }

	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void SetShutterSpeed(double NewShutterSpeed);

	/** ISO 感光度 */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	int32 GetIso() const { return Iso; }

	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void SetIso(int32 NewIso);

	/** 曝光补偿（EV，如 -2.0 ~ 2.0） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	double GetExposureCompensation() const { return ExposureCompensation; }

	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void SetExposureCompensation(double NewCompensation);

	/** 对焦模式（0 手动 / 1 单次自动 / 2 连续自动，对齐 focus_mode） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	int32 GetFocusMode() const { return FocusMode; }

	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void SetFocusMode(int32 NewMode);

	/** 对焦值（0-100，手动对焦位置） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	int32 GetFocusValue() const { return FocusValue; }

	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void SetFocusValue(int32 NewValue);

	/** 点对焦动作（point_focus_start / point_focus_stop，仅记录最近动作） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	FString GetPointFocusAction() const { return PointFocusAction; }

	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void SetPointFocusAction(const FString& InAction);

	/** 红外测光模式（0 全局 / 1 点测光 / 2 区域测光） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	int32 GetIrMeteringMode() const { return IrMeteringMode; }

	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void SetIrMeteringMode(int32 NewMode);

	/** 红外测光点（归一化坐标 0-1） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	void GetIrMeteringPoint(double& OutX, double& OutY) const { OutX = IrMeteringPointX; OutY = IrMeteringPointY; }

	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void SetIrMeteringPoint(double InX, double InY);

	/** 红外测光区域（归一化坐标：中心 x/y、宽高 w/h） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	void GetIrMeteringArea(double& OutX, double& OutY, double& OutW, double& OutH) const { OutX = IrMeteringAreaX; OutY = IrMeteringAreaY; OutW = IrMeteringAreaW; OutH = IrMeteringAreaH; }

	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void SetIrMeteringArea(double InX, double InY, double InW, double InH);

	/** 照片存储位置（如 current / sd_card，OSD 数组首元素） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	FString GetPhotoStorageLocation() const { return PhotoStorageLocation; }

	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void SetPhotoStorageLocation(const FString& InLocation);

	/** 录像存储位置 */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	FString GetVideoStorageLocation() const { return VideoStorageLocation; }

	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void SetVideoStorageLocation(const FString& InLocation);

	/** 分屏使能（camera_screen_split 指令设置，OSD screen_split_enable） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	bool IsScreenSplitEnabled() const { return bScreenSplitEnabled; }

	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void SetScreenSplitEnabled(bool bInEnabled) { bScreenSplitEnabled = bInEnabled; }

	/** 焦距（毫米，等效 35mm） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	double GetFocalLength() const { return FocalLength; }

	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void SetFocalLength(double NewFocalLength);

	/** 看点目标（camera_look_at 经纬度/海拔）；未设置返回 false */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	bool GetLookAtTarget(FUAVGeoCoordinate& OutTarget) const;

	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void SetLookAtTarget(const FUAVGeoCoordinate& InTarget);

	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void ClearLookAtTarget();

	/** POI 环绕模式是否激活（poi_mode_enter 后为 true，不驱动飞行物理） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	bool IsPoiModeActive() const { return bPoiModeActive; }

	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void SetPoiModeActive(bool bInActive) { bPoiModeActive = bInActive; }

	/** POI 环绕最大速度（米/秒） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	double GetPoiMaxSpeed() const { return PoiMaxSpeed; }

	/** POI 环绕云台偏航角速度（度/秒） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	double GetPoiGimbalYawRate() const { return PoiGimbalYawRate; }

	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void SetPoiCircleSpeed(double InMaxSpeed, double InGimbalYawRate);

	/** 是否拍照中 */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	bool IsPhotoTaking() const { return bPhotoTaking; }

	/** 剩余照片数（初始 9999，每次拍照减 1，最低 0） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	int32 GetRemainingPhotoNum() const { return RemainingPhotoNum; }

	/** 累计已拍照片数 */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	int32 GetTakenPhotoCount() const { return TakenPhotoCount; }

	/** 拍照：剩余照片数减 1（最低 0）、累计加 1，进入拍照中状态（3 秒后自动结束） */
	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void TakePhoto();

	/** 设置拍照中状态（camera_photo_stop 以 false 结束拍照） */
	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void SetPhotoTaking(bool bInPhotoTaking);

	/** 录像指令覆盖：开始录像（指令优先于飞行模式推导） */
	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void StartRecording();

	/** 录像指令覆盖：停止录像 */
	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void StopRecording();

	/** 清除录像指令覆盖，恢复按飞行模式推导 */
	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void ClearRecordingOverride();

	/** 设置云台指令目标（覆盖时间微动；俯仰为有符号角，偏航按 0-360 归一化） */
	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void SetGimbalTarget(double InPitchDegrees, double InYawDegrees);

	/** 清除云台指令目标，恢复时间微动推导 */
	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void ResetGimbalTarget();

	/** 是否设置了云台指令目标 */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	bool HasGimbalTarget() const { return bHasGimbalTarget; }

	/** 云台指令目标俯仰角（度） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	double GetGimbalTargetPitch() const { return GimbalTargetPitch; }

	/** 云台指令目标偏航角（度） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	double GetGimbalTargetYaw() const { return GimbalTargetYaw; }

	/** 设置载荷权（payload_authority_grab 抢占后为 true） */
	UFUNCTION(BlueprintCallable, Category = "UAV|Payload")
	void SetPayloadAuthority(bool bInHasAuthority) { bHasPayloadAuthority = bInHasAuthority; }

	/** 是否已抢占载荷权 */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	bool HasPayloadAuthority() const { return bHasPayloadAuthority; }

	/** 是否录像中（起飞/航线/返航状态，对齐 dock 模式编码集合） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	bool IsRecording() const;

	/** 累计飞行距离（米） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	double GetTotalFlightDistanceMeters() const { return TotalFlightDistanceMeters; }

	/** 累计飞行时长（秒） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	double GetTotalFlightTimeSeconds() const { return TotalFlightTimeSeconds; }

	/** 累计录制时长（秒） */
	UFUNCTION(BlueprintPure, Category = "UAV|Payload")
	double GetRecordingTimeSeconds() const { return RecordingTimeSeconds; }

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

	/** 当前电量百分比 */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	double BatteryCapacityPercent = 100.0;

	/** 模拟累计秒（云台微动时间源） */
	UPROPERTY()
	double ElapsedSimTimeSeconds = 0.0;

	/** 云台角度状态 */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	FUAVGimbalState GimbalState;

	/** 当前变焦倍率 */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	double ZoomFactor = 3.0;

	/** 拍照中（photo_state 输出源） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	bool bPhotoTaking = false;

	/** 剩余照片数（初始 9999，对齐 dock 口径） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	int32 RemainingPhotoNum = 9999;

	/** 累计已拍照片数 */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	int32 TakenPhotoCount = 0;

	/** 拍照剩余时长（秒，到 0 自动结束单张拍摄） */
	UPROPERTY()
	double PhotoTakingRemainingSeconds = 0.0;

	/** 录像指令覆盖是否已设置 */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	bool bRecordingOverrideSet = false;

	/** 录像指令覆盖值（bRecordingOverrideSet 为真时生效） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	bool bRecordingOverrideValue = false;

	/** 云台指令目标是否已设置 */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	bool bHasGimbalTarget = false;

	/** 云台指令目标俯仰角（度，有符号） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	double GimbalTargetPitch = 0.0;

	/** 云台指令目标偏航角（度，输出时归一化到 0-360） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	double GimbalTargetYaw = 0.0;

	/** 是否已抢占载荷权 */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	bool bHasPayloadAuthority = false;

	/** 曝光模式（0 自动 / 1 手动 / 2 快门优先 / 3 光圈优先） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	int32 ExposureMode = 0;

	/** 快门速度（秒） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	double ShutterSpeed = 1.0 / 1000.0;

	/** ISO 感光度 */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	int32 Iso = 100;

	/** 曝光补偿（EV） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	double ExposureCompensation = 0.0;

	/** 对焦模式（0 手动 / 1 单次自动 / 2 连续自动） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	int32 FocusMode = 0;

	/** 对焦值（0-100） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	int32 FocusValue = 0;

	/** 最近点对焦动作（point_focus_start / point_focus_stop） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	FString PointFocusAction;

	/** 红外测光模式（0 全局 / 1 点测光 / 2 区域测光） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	int32 IrMeteringMode = 0;

	/** 红外测光点（归一化 0-1） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	double IrMeteringPointX = 0.5;

	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	double IrMeteringPointY = 0.5;

	/** 红外测光区域（归一化：中心 x/y、宽 w、高 h） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	double IrMeteringAreaX = 0.5;

	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	double IrMeteringAreaY = 0.5;

	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	double IrMeteringAreaW = 0.2;

	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	double IrMeteringAreaH = 0.2;

	/** 照片存储位置（current / sd_card 等） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	FString PhotoStorageLocation = TEXT("current");

	/** 录像存储位置 */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	FString VideoStorageLocation = TEXT("current");

	/** 分屏使能（OSD screen_split_enable） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	bool bScreenSplitEnabled = false;

	/** 焦距（毫米） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	double FocalLength = 24.0;

	/** 看点目标是否已设置 */
	UPROPERTY()
	bool bHasLookAtTarget = false;

	/** 看点目标（经纬度/海拔） */
	UPROPERTY()
	FUAVGeoCoordinate LookAtTarget;

	/** POI 环绕模式激活 */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	bool bPoiModeActive = false;

	/** POI 环绕最大速度（米/秒） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	double PoiMaxSpeed = 5.0;

	/** POI 环绕云台偏航角速度（度/秒） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	double PoiGimbalYawRate = 30.0;

	/** 累计飞行距离（米） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	double TotalFlightDistanceMeters = 0.0;

	/** 累计飞行时长（秒） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	double TotalFlightTimeSeconds = 0.0;

	/** 累计录制时长（秒） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Payload", meta = (AllowPrivateAccess = "true"))
	double RecordingTimeSeconds = 0.0;

	/** 低电量事件已触发标志（回升后复位） */
	UPROPERTY()
	bool bBatteryLowEventFired = false;
};
