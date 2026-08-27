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

	// ---- 指令分发 ----
	/** 解析 services 报文并按 method 分发到飞控/相机，回发 services_reply（InSn 为报文来源设备 SN，空则回退机场 SN） */
	void DispatchServicesMessage(const FString& InPayloadJson, const FString& InSn = FString());

	// ---- 发布 ----
	/** 发布 services_reply（thing/product/{sn}/services_reply，InSn 空则用机场 SN） */
	void PublishServicesReply(const FString& InMethod, const FString& InTid, const FString& InBid, int32 InResult, const FString& InSn = FString());

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

	// ---- OSD 组装 ----
	/** 从无人机模拟组件读取遥测，组装无人机 OSD data（对齐 dock report_drone_osd.py） */
	TSharedPtr<FJsonObject> BuildDroneOsdPayload() const;

	/** 飞行状态 → 上云 API mode_code */
	int32 FlightStateToModeCode(EUAVFlightState InState) const;

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

	/** 已连接 */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|MqttBridge", meta = (AllowPrivateAccess = "true"))
	bool bConnected = false;

	/** OSD 周期计时累加器（秒） */
	double OsdAccumulator = 0.0;
};
