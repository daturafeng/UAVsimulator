// 上云 API MQTT 桥接：订阅 dock 下发的 services 指令，分发到飞控/相机，回发回复/事件/OSD
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Dom/JsonObject.h"
#include "UAVDroneSimComponent.h"
#include "UAVMqttBridgeComponent.generated.h"

class UUAVFlightControlComponent;
class UUAVDroneSimComponent;
class UUAVCameraStreamComponent;
class UMQTTClientObject;
class UMQTTSubscriptionObject;

struct FMQTTClientMessage;

/** MQTT 连接状态变更事件（参数：是否已连接） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUAVMqttConnectionChangedDelegate, bool, bConnected);

/** 收到 services 指令事件（调试/转发用；参数：method） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUAVServiceCommandReceivedDelegate, const FString&, Method);

/**
 * MQTT 桥接组件：通过引擎自带 MQTTCore 插件连接 dock 的 MQTT broker，
 * 订阅 thing/product/{机场SN}/services 接收指令并分发到 UAVFlightControl /
 * UAVCameraStream，回发 services_reply；转发进度/直播状态事件到 events topic；
 * 周期上报无人机/机场 OSD 与设备状态。
 */
UCLASS(ClassGroup = (UAV), meta = (BlueprintSpawnableComponent))
class UAVMQTTBRIDGE_API UUAVMqttBridgeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUAVMqttBridgeComponent();

	// ---- 依赖注入（BeginPlay 前调用） ----
	/** 注入飞控组件 */
	UFUNCTION(BlueprintCallable, Category = "UAV|MqttBridge")
	void SetFlightControl(UUAVFlightControlComponent* InFlightControl);

	/** 注入无人机模拟组件（OSD 遥测来源） */
	UFUNCTION(BlueprintCallable, Category = "UAV|MqttBridge")
	void SetDroneSim(UUAVDroneSimComponent* InDroneSim);

	/** 注入相机载荷组件（直播指令分发与状态查询） */
	UFUNCTION(BlueprintCallable, Category = "UAV|MqttBridge")
	void SetCameraStream(UUAVCameraStreamComponent* InCameraStream);

	/** 组装机场 OSD data（对齐 dock OsdDock 完整字段；自动化测试入口） */
	TSharedPtr<FJsonObject> BuildDockOsdPayload() const;

	/** 组装直播能力 data（data.live_capacity，对齐 dock DockLiveCapacity / report_live_capacity.py；自动化测试入口） */
	TSharedPtr<FJsonObject> BuildLiveCapacityPayload() const;

	/** 组装 flighttask_progress 事件 data（对齐 dock EventsDataRequest<FlighttaskProgress>；自动化测试入口） */
	TSharedPtr<FJsonObject> BuildFlighttaskProgressEventData(const FString& InStatus, const FString& InFlightId, int32 InCurrentWaypointIndex, int32 InPercent) const;

	/** 组装 return_home_info 事件 data（对齐 dock ReturnHomeInfo；自动化测试入口） */
	TSharedPtr<FJsonObject> BuildReturnHomeInfoEventData() const;

	/** 组装 flighttask_ready 事件 data（data.flight_ids；自动化测试入口） */
	TSharedPtr<FJsonObject> BuildFlighttaskReadyData(const FString& InFlightId) const;

	/** 组装 fly_to_point_progress 事件 data（对齐 dock FlyToPointProgress：data = { result, status, fly_to_id, way_point_index }；自动化测试入口） */
	TSharedPtr<FJsonObject> BuildFlyToPointProgressEventData(const FString& InStatus, const FString& InFlyToId, int32 InWayPointIndex, int32 InResult) const;

	/** 组装 hms 事件 data（data.list；InLowBatteryAlarm 为 true 时含低电量告警；自动化测试入口） */
	TSharedPtr<FJsonObject> BuildHmsPayload(bool bLowBatteryAlarm = false) const;

	/** 组装 drc/up 回执 data 报文（对齐 dock DrcUpData：data={result, output?:{seq}}；InSeq<0 时不带 output；自动化测试入口） */
	TSharedPtr<FJsonObject> BuildDrcUpReply(const FString& InMethod, const FString& InTid, const FString& InBid, int32 InResult, int32 InSeq) const;

	/** 组装 drc_status_notify 事件 data（对齐 dock DrcStateEnum：data={result:0, drc_state}；自动化测试入口） */
	TSharedPtr<FJsonObject> BuildDrcStatusNotifyData(int32 InDrcState) const;

	/** 是否为远程调试/设备控制指令（精确匹配 20 个调试 method，对齐 dock DebugMethodEnum；自动化测试入口） */
	static bool IsRemoteDebugMethod(const FString& InMethod);

	/** 远程调试指令分发入口：校验带参指令并更新机场设备状态，返回 result（对齐 dock AbstractDebugService；自动化测试入口） */
	int32 HandleDebugCommand(const FString& InMethod, const FString& InDataJson);

	/** 组装 RemoteDebugProgress 进度事件 data（对齐 dock RemoteDebugProgress：data={result:0, output:{status, progress:{percent, currentStep, totalSteps, stepKey?, stepResult}}}；自动化测试入口） */
	TSharedPtr<FJsonObject> BuildRemoteDebugProgressEventData(const FString& InStatus, int32 InPercent, int32 InCurrentStep, int32 InTotalSteps, const FString& InStepKey, int32 InStepResult) const;

	/** 组装 ota_progress 事件 data（对齐 dock EventsDataRequest<OtaProgress>：data={result:0, output:{status, progress:{percent, current_step}, ext:{rate}}}；自动化测试入口） */
	TSharedPtr<FJsonObject> BuildOtaProgressEventData(const FString& InStatus, int32 InPercent, int32 InCurrentStep, int32 InRate) const;

	/** OTA 固件升级指令处理：校验 devices（1-2 个设备、字段齐全合法），记录目标版本并置升级中，返回 result（对齐 dock AbstractFirmwareService.otaCreate；自动化测试入口） */
	int32 HandleOtaCreate(const FString& InMethod, const FString& InDataJson);

	/** 完成 OTA 升级：目标版本落地为当前版本并复位升级标志（ok 事件后调用；自动化测试入口） */
	void CompleteOtaUpgrade();

	/** 组装机场固件版本 state data（对齐 dock DockFirmwareVersion：data={firmware_version, compatible_status, firmware_upgrade_status}；自动化测试入口） */
	TSharedPtr<FJsonObject> BuildDockFirmwareVersionData() const;

	/** 组装无人机固件版本 state data（对齐 dock FirmwareVersion：data={firmware_version}；自动化测试入口） */
	TSharedPtr<FJsonObject> BuildDroneFirmwareVersionData() const;

	/** 组装载荷固件版本 state data（对齐 dock PayloadFirmwareVersion：data={载荷索引:{firmware_version}}；自动化测试入口） */
	TSharedPtr<FJsonObject> BuildPayloadFirmwareVersionData() const;

	// ---- 配置（默认值对齐 dock 联调环境） ----
	/** Broker 地址 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|MqttBridge|Config")
	FString BrokerAddress = TEXT("10.100.51.15");

	/** Broker 端口 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|MqttBridge|Config")
	int32 Port = 1883;

	/** 用户名 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|MqttBridge|Config")
	FString Username = TEXT("root");

	/** 密码 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|MqttBridge|Config")
	FString Password = TEXT("unis@123");

	/** 机场 SN（dock3，接收指令的 topic 主体） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|MqttBridge|Config")
	FString DockSn = TEXT("DOCK3TEST001");

	/** 无人机 SN（M4TD） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|MqttBridge|Config")
	FString DroneSn = TEXT("1581F8HGXTEST001");

	/** 相机载荷索引 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|MqttBridge|Config")
	FString CameraIndex = TEXT("52-0-0");

	/** OSD 上报周期（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|MqttBridge|Config", meta = (ClampMin = "0.1"))
	float OsdIntervalSeconds = 1.0f;

	/** BeginPlay 时自动连接 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|MqttBridge|Config")
	bool bAutoConnectOnBeginPlay = true;

	// ---- API ----
	/** 建立 MQTT 连接（异步，结果通过 OnConnectionChanged 广播） */
	UFUNCTION(BlueprintCallable, Category = "UAV|MqttBridge")
	bool Connect();

	/** 断开 MQTT 连接 */
	UFUNCTION(BlueprintCallable, Category = "UAV|MqttBridge")
	void Disconnect();

	/** 是否已连接 */
	UFUNCTION(BlueprintPure, Category = "UAV|MqttBridge")
	bool IsConnected() const { return bConnected; }

	/** 向指定 topic 发布原始 JSON（调试/扩展用） */
	UFUNCTION(BlueprintCallable, Category = "UAV|MqttBridge")
	void PublishRaw(const FString& InTopic, const FString& InPayloadJson);

	// ---- 事件 ----
	/** 连接状态变更事件 */
	UPROPERTY(BlueprintAssignable, Category = "UAV|MqttBridge|Event")
	FUAVMqttConnectionChangedDelegate OnConnectionChanged;

	/** 收到 services 指令事件（method） */
	UPROPERTY(BlueprintAssignable, Category = "UAV|MqttBridge|Event")
	FUAVServiceCommandReceivedDelegate OnServiceCommandReceived;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---- MQTT 回调（UFUNCTION 供动态委托绑定） ----
	UFUNCTION()
	void OnMqttConnect(EMQTTConnectReturnCode ReturnCode);

	UFUNCTION()
	void OnMqttDisconnect();

	UFUNCTION()
	void OnServicesMessage(const FMQTTClientMessage& InMessage);

	UFUNCTION()
	void OnDrcMessage(const FMQTTClientMessage& InMessage);

	// ---- 指令分发 ----
	/** 解析 services 报文并按 method 分发到飞控/相机，回发 services_reply（InSn 为报文来源设备 SN，空则回退机场 SN） */
	void DispatchServicesMessage(const FString& InPayloadJson, const FString& InSn = FString());

	/** 解析 drc/down 报文并按 method 分发到飞控，回发 drc/up（InSn 为报文来源设备 SN，空则回退机场 SN） */
	void DispatchDrcMessage(const FString& InPayloadJson, const FString& InSn = FString());

	// ---- 发布 ----
	/** 发布 services_reply（thing/product/{sn}/services_reply，InSn 空则用机场 SN；InOutput 非空时 data 附带 output 字段，如远程调试成功回执 {status:"sent"}） */
	void PublishServicesReply(const FString& InMethod, const FString& InTid, const FString& InBid, int32 InResult, const FString& InSn = FString(), const TSharedPtr<FJsonObject>& InOutput = nullptr);

	/** 发布 drc/up 回执（thing/product/{sn}/drc/up，InSn 空则用机场 SN；drone_control/heart_beat 带 output.seq，drone_emergency_stop 仅 result） */
	void PublishDrcUpReply(const FString& InMethod, const FString& InTid, const FString& InBid, int32 InResult, const FString& InSn = FString());

	/** 发布事件（thing/product/{DockSn}/events，含 gateway） */
	void PublishEvent(const FString& InMethod, const TSharedPtr<FJsonObject>& InData, const FString& InTid = FString(), const FString& InBid = FString());

	/** 发布无人机 OSD（thing/product/{DroneSn}/osd） */
	void PublishDroneOsd();

	/** 发布机场 OSD（thing/product/{DockSn}/osd，精简字段） */
	void PublishDockOsd();

	/** 发布设备状态（thing/product/{sn}/state，online/offline） */
	void PublishDeviceState(const FString& InSn, bool bOnline);

	/** 发布在线状态（sys/product/{DockSn}/status） */
	void PublishOnlineStatus(bool bOnline);

	/** 发布载荷控制源（thing/product/{DroneSn}/state，对齐 dock report_control_source.py） */
	void PublishPayloadControlSource();

	/** 发布直播能力（thing/product/{DockSn}/state，对齐 dock report_live_capacity.py） */
	void PublishLiveCapacity();

	/** 发布机场固件版本 state（thing/product/{DockSn}/state，对齐 dock DockStateDataKeyEnum.FIRMWARE_VERSION） */
	void PublishDockFirmwareVersion();

	/** 发布无人机固件版本 state（thing/product/{DroneSn}/state，对齐 dock RcStateDataKeyEnum.FIRMWARE_VERSION） */
	void PublishDroneFirmwareVersion();

	/** 发布载荷固件版本 state（thing/product/{DroneSn}/state，对齐 dock PayloadFirmwareVersion） */
	void PublishPayloadFirmwareVersion();

	/** 发布 hms 告警事件（thing/product/{DockSn}/events，method=hms） */
	void PublishHms(const TSharedPtr<FJsonObject>& InHmsData);

	// ---- 事件回调（BeginPlay 时绑定飞控/相机委托） ----
	UFUNCTION()
	void OnFlightCommandResult(const FString& InMethod, int32 InResult);

	UFUNCTION()
	void OnTakeoffProgress(const FString& InStatus, const FString& InFlightId, int32 InWayPointIndex, double InRemainingDistance);

	UFUNCTION()
	void OnFlighttaskProgress(const FString& InStatus, const FString& InFlightId, int32 InCurrentWaypointIndex, int32 InPercent);

	UFUNCTION()
	void OnLiveStatusChanged(const FString& InVideoId);

	UFUNCTION()
	void OnLiveCommandResult(const FString& InMethod, int32 InResult);

	UFUNCTION()
	void OnReturnHomeStatus(const FString& InStatus, const FString& InReason);

	UFUNCTION()
	void OnFlighttaskReady(const FString& InFlightId);

	UFUNCTION()
	void OnFlyToPointProgress(const FString& InStatus, const FString& InFlyToId, int32 InWayPointIndex, int32 InResult);

	UFUNCTION()
	void OnDrcStatusNotify(int32 InDrcState);

	// ---- OSD 组装 ----
	/** 从无人机模拟组件读取遥测，组装无人机 OSD data（对齐 dock report_drone_osd.py） */
	TSharedPtr<FJsonObject> BuildDroneOsdPayload() const;

	/** 飞行状态 → 上云 API mode_code */
	int32 FlightStateToModeCode(EUAVFlightState InState) const;

protected:
	/** 归巢待命判定：无人机在机场原点 ±0.00002 度内、高度 ≤12 且待机 */
	bool IsDroneInDock() const;

	/** 飞行状态 → 上云 API flighttask_step_code（对齐 dock report_dock_osd.py） */
	int32 GetFlightTaskStepCode() const;

	/** 机场是否处于任务中（无人机非待机即视为机场执行任务） */
	bool IsDockInMission() const;

private:
	/** 关联的飞控组件 */
	UPROPERTY()
	TObjectPtr<UUAVFlightControlComponent> FlightControl;

	/** 关联的无人机模拟组件 */
	UPROPERTY()
	TObjectPtr<UUAVDroneSimComponent> DroneSim;

	/** 关联的相机载荷组件 */
	UPROPERTY()
	TObjectPtr<UUAVCameraStreamComponent> CameraStream;

	/** MQTT 客户端对象 */
	UPROPERTY(Transient)
	TObjectPtr<UMQTTClientObject> MqttClient;

	/** services 订阅对象 */
	UPROPERTY(Transient)
	TObjectPtr<UMQTTSubscriptionObject> ServicesSubscription;

	/** drc/down 订阅对象 */
	UPROPERTY(Transient)
	TObjectPtr<UMQTTSubscriptionObject> DrcSubscription;

	/** 已连接 */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|MqttBridge", meta = (AllowPrivateAccess = "true"))
	bool bConnected = false;

	/** OSD 周期计时累加器（秒） */
	double OsdAccumulator = 0.0;

	// ---- 机场设备模拟状态（远程调试指令驱动，OSD 联动输出）----
	/** 调试模式已激活（debug_mode_open/close） */
	UPROPERTY()
	bool bDebugMode = false;

	/** 补光灯开启（supplement_light_open/close） */
	UPROPERTY()
	bool bSupplementLight = false;

	/** 声光报警开启（alarm_state_switch） */
	UPROPERTY()
	bool bAlarmState = false;

	/** 推杆松开（putter_open/close） */
	UPROPERTY()
	bool bPutterOpen = false;

	/** 舱门指令覆盖生效（cover_open/close 后为 true，覆盖归巢推导） */
	UPROPERTY()
	bool bCoverOverride = false;

	/** 舱门打开状态（指令覆盖值：cover_open=1 / cover_close=0） */
	UPROPERTY()
	bool bCoverOpen = false;

	/** 充电指令覆盖生效（charge_open/close 后为 true，覆盖电量推导） */
	UPROPERTY()
	bool bChargeOverride = false;

	/** 充电打开状态（指令覆盖值：charge_open=1 / charge_close=0） */
	UPROPERTY()
	bool bChargeOpen = false;

	/** 电池存储模式（battery_store_mode_switch：1=PLAN 计划存储 / 2=EMERGENCY 应急存储） */
	UPROPERTY()
	int32 BatteryStoreMode = 1;

	/** 空调模式（air_conditioner_mode_switch：0=待机 / 1=制冷 / 2=制热 / 3=除湿） */
	UPROPERTY()
	int32 AirConditionerState = 0;

	/** 电池保养开启（battery_maintenance_switch） */
	UPROPERTY()
	bool bBatteryMaintenance = false;

	/** 链路工作模式（sdr_workmode_switch：0=SDR 仅 SDR / 1=SDR+4G 双链路） */
	UPROPERTY()
	int32 LinkWorkmode = 1;

	/** 无人机电源开启（drone_open/close，联动 OSD 子设备在线状态） */
	UPROPERTY()
	bool bDronePowerOn = true;

	// ---- OTA 固件升级模拟状态（ota_create 指令驱动，固件版本 state 联动输出）----
	/** 升级目标版本（ota_create 的 product_version；ok 事件后落地为当前版本） */
	UPROPERTY()
	FString OtaTargetVersion;

	/** 升级进行中（ok 事件前为 true，固件版本 state 的 firmware_upgrade_status 联动） */
	UPROPERTY()
	bool bOtaUpgrading = false;

	/** 机场当前固件版本（默认占位，升级完成后更新为目标版本） */
	UPROPERTY()
	FString DockFirmwareVersion = TEXT("03.01.0000");

	/** 无人机当前固件版本（默认占位，升级完成后更新为目标版本） */
	UPROPERTY()
	FString DroneFirmwareVersion = TEXT("03.01.0000");

	/** 载荷当前固件版本（默认占位，随整包升级更新） */
	UPROPERTY()
	FString PayloadFirmwareVersion = TEXT("03.01.0000");
};
