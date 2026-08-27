// 上云 API MQTT 桥接组件实现
#include "UAVMqttBridgeComponent.h"

#include "MQTTClientMessage.h"
#include "MQTTClientObject.h"
#include "MQTTShared.h"
#include "MQTTSubsystem.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "UAVCameraStreamComponent.h"
#include "UAVCloudApiTypes.h"
#include "UAVDroneSimComponent.h"
#include "UAVFlightControlComponent.h"
#include "UAVPayloadMath.h"

#include "Misc/DateTime.h"

// 上云 API 报文工具位于 UAV::CloudApi 命名空间，统一引入
using namespace UAV::CloudApi;

namespace
{
	/** 将 JSON 对象序列化为紧凑字符串（失败返回空串） */
	FString SerializeJson(const TSharedRef<FJsonObject>& InRoot)
	{
		FString OutString;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutString);
		if (FJsonSerializer::Serialize(InRoot, Writer))
		{
			return OutString;
		}
		return FString();
	}

	/**
	 * 从 services topic（thing/product/{sn}/services）提取设备 SN。
	 * 提取失败返回空串，由调用方回退默认 SN。
	 */
	FString ParseSnFromServicesTopic(const FString& InTopic)
	{
		const FString Prefix = TEXT("thing/product/");
		const FString Suffix = TEXT("/services");
		if (InTopic.StartsWith(Prefix) && InTopic.EndsWith(Suffix))
		{
			return InTopic.Mid(Prefix.Len(), InTopic.Len() - Prefix.Len() - Suffix.Len());
		}
		return FString();
	}
}

UUAVMqttBridgeComponent::UUAVMqttBridgeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UUAVMqttBridgeComponent::SetFlightControl(UUAVFlightControlComponent* InFlightControl)
{
	FlightControl = InFlightControl;
}

void UUAVMqttBridgeComponent::SetDroneSim(UUAVDroneSimComponent* InDroneSim)
{
	DroneSim = InDroneSim;
}

void UUAVMqttBridgeComponent::SetCameraStream(UUAVCameraStreamComponent* InCameraStream)
{
	CameraStream = InCameraStream;
}

void UUAVMqttBridgeComponent::BeginPlay()
{
	Super::BeginPlay();

	// 未显式注入时，从拥有者身上自动获取依赖组件，提升部署鲁棒性
	if (FlightControl == nullptr)
	{
		FlightControl = GetOwner() ? GetOwner()->GetComponentByClass<UUAVFlightControlComponent>() : nullptr;
	}
	if (DroneSim == nullptr)
	{
		DroneSim = GetOwner() ? GetOwner()->GetComponentByClass<UUAVDroneSimComponent>() : nullptr;
	}
	if (CameraStream == nullptr)
	{
		CameraStream = GetOwner() ? GetOwner()->GetComponentByClass<UUAVCameraStreamComponent>() : nullptr;
	}

	// 绑定飞控/相机事件委托，回调转发为上云 API 事件
	if (FlightControl)
	{
		FlightControl->OnCommandResult.AddDynamic(this, &UUAVMqttBridgeComponent::OnFlightCommandResult);
		FlightControl->OnTakeoffProgress.AddDynamic(this, &UUAVMqttBridgeComponent::OnTakeoffProgress);
		FlightControl->OnFlighttaskProgress.AddDynamic(this, &UUAVMqttBridgeComponent::OnFlighttaskProgress);
	}
	if (CameraStream)
	{
		CameraStream->OnLiveStatusChanged.AddDynamic(this, &UUAVMqttBridgeComponent::OnLiveStatusChanged);
		CameraStream->OnCommandResult.AddDynamic(this, &UUAVMqttBridgeComponent::OnLiveCommandResult);
	}

	if (bAutoConnectOnBeginPlay)
	{
		Connect();
	}
}

void UUAVMqttBridgeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 解绑委托，防止悬垂引用
	if (FlightControl)
	{
		FlightControl->OnCommandResult.RemoveDynamic(this, &UUAVMqttBridgeComponent::OnFlightCommandResult);
		FlightControl->OnTakeoffProgress.RemoveDynamic(this, &UUAVMqttBridgeComponent::OnTakeoffProgress);
		FlightControl->OnFlighttaskProgress.RemoveDynamic(this, &UUAVMqttBridgeComponent::OnFlighttaskProgress);
	}
	if (CameraStream)
	{
		CameraStream->OnLiveStatusChanged.RemoveDynamic(this, &UUAVMqttBridgeComponent::OnLiveStatusChanged);
		CameraStream->OnCommandResult.RemoveDynamic(this, &UUAVMqttBridgeComponent::OnLiveCommandResult);
	}

	Disconnect();
	Super::EndPlay(EndPlayReason);
}

void UUAVMqttBridgeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bConnected)
	{
		return;
	}

	// 周期上报 OSD 遥测
	OsdAccumulator += DeltaTime;
	if (OsdAccumulator >= OsdIntervalSeconds)
	{
		OsdAccumulator = 0.0;
		PublishDroneOsd();
		PublishDockOsd();
	}
}

bool UUAVMqttBridgeComponent::Connect()
{
	if (bConnected)
	{
		return true;
	}

	if (BrokerAddress.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[UAVMqttBridge] Broker 地址为空，无法连接"));
		return false;
	}

	const FMQTTURL URL(BrokerAddress, Port, Username, Password, EMQTTScheme::MQTT);
	MqttClient = UMQTTSubsystem::GetOrCreateClient(this, URL);
	if (!MqttClient)
	{
		UE_LOG(LogTemp, Error, TEXT("[UAVMqttBridge] 创建 MQTT 客户端失败"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[UAVMqttBridge] 正在连接 %s:%d ..."), *BrokerAddress, Port);
	UMQTTClientObject::FOnConnectDelegate ConnectDelegate;
	ConnectDelegate.BindUFunction(this, TEXT("OnMqttConnect"));
	MqttClient->Connect(ConnectDelegate);
	return true;
}

void UUAVMqttBridgeComponent::Disconnect()
{
	if (!MqttClient)
	{
		return;
	}
	UMQTTClientObject::FOnDisconnectDelegate DisconnectDelegate;
	DisconnectDelegate.BindUFunction(this, TEXT("OnMqttDisconnect"));
	MqttClient->Disconnect(DisconnectDelegate);
	MqttClient = nullptr;
	ServicesSubscription = nullptr;
	bConnected = false;
}

void UUAVMqttBridgeComponent::OnMqttConnect(EMQTTConnectReturnCode ReturnCode)
{
	if (ReturnCode == EMQTTConnectReturnCode::Accepted)
	{
		bConnected = true;
		UE_LOG(LogTemp, Log, TEXT("[UAVMqttBridge] 连接成功，订阅 %s/services"), *DockSn);

		// 订阅机场 services 指令 topic
		const FString ServicesTopic = MakeTopic(kTopicServicesTemplate, DockSn);
		ServicesSubscription = MqttClient->Subscribe(ServicesTopic, EMQTTQualityOfService::Once);
		if (ServicesSubscription)
		{
			UMQTTSubscriptionObject::FOnMessageDelegate MessageDelegate;
			MessageDelegate.BindUFunction(this, TEXT("OnServicesMessage"));
			ServicesSubscription->SetOnMessageHandler(MessageDelegate);
		}

		// 上报上线状态
		PublishOnlineStatus(true);
		PublishDeviceState(DockSn, true);
		PublishDeviceState(DroneSn, true);

		OnConnectionChanged.Broadcast(true);
	}
	else
	{
		bConnected = false;
		UE_LOG(LogTemp, Warning, TEXT("[UAVMqttBridge] 连接失败，返回码 %d"), static_cast<int32>(ReturnCode));
		OnConnectionChanged.Broadcast(false);
	}
}

void UUAVMqttBridgeComponent::OnMqttDisconnect()
{
	if (bConnected)
	{
		bConnected = false;
		UE_LOG(LogTemp, Log, TEXT("[UAVMqttBridge] 连接断开"));
		PublishOnlineStatus(false);
		PublishDeviceState(DockSn, false);
		PublishDeviceState(DroneSn, false);
		OnConnectionChanged.Broadcast(false);
	}
}

void UUAVMqttBridgeComponent::OnServicesMessage(const FMQTTClientMessage& InMessage)
{
	const FString& PayloadJson = InMessage.GetPayloadAsString();
	if (PayloadJson.IsEmpty())
	{
		return;
	}
	// 提取报文来源设备 SN（topic 形如 thing/product/{sn}/services）
	const FString SourceSn = ParseSnFromServicesTopic(InMessage.Topic);
	DispatchServicesMessage(PayloadJson, SourceSn);
}

void UUAVMqttBridgeComponent::DispatchServicesMessage(const FString& InPayloadJson, const FString& InSn)
{
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(InPayloadJson), Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[UAVMqttBridge] services 报文解析失败: %s"), *InPayloadJson);
		return;
	}

	const FString Tid = Root->GetStringField(TEXT("tid"));
	const FString Bid = Root->GetStringField(TEXT("bid"));
	const FString Method = Root->GetStringField(TEXT("method"));
	if (Method.IsEmpty())
	{
		return;
	}

	// 取 data 字段并序列化为 JSON 字符串传给业务组件
	FString DataJson;
	if (Root->HasField(TEXT("data")))
	{
		const TSharedPtr<FJsonObject> DataObj = Root->GetObjectField(TEXT("data"));
		if (DataObj.IsValid())
		{
			DataJson = SerializeJson(DataObj.ToSharedRef());
		}
	}

	int32 Result = UAV::FlightControlResult::UnknownMethod;
	const bool bIsFlightCommand = Method.StartsWith(TEXT("flight_")) || Method.StartsWith(TEXT("takeoff_"))
		|| Method.StartsWith(TEXT("return_home"));
	const bool bIsLiveCommand = Method.StartsWith(TEXT("live_"));

	if (bIsFlightCommand && FlightControl)
	{
		Result = FlightControl->HandleCommand(Method, DataJson);
	}
	else if (bIsLiveCommand && CameraStream)
	{
		Result = CameraStream->HandleCommand(Method, DataJson);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[UAVMqttBridge] 未知或未支持的指令: %s"), *Method);
	}

	// 回发 services_reply（按来源设备 SN，缺省回退机场 SN）
	PublishServicesReply(Method, Tid, Bid, Result, InSn);
	OnServiceCommandReceived.Broadcast(Method);
}

void UUAVMqttBridgeComponent::PublishServicesReply(const FString& InMethod, const FString& InTid, const FString& InBid, int32 InResult, const FString& InSn)
{
	if (!MqttClient || !bConnected)
	{
		return;
	}
	const TSharedRef<FJsonObject> Reply = MakeServicesReply(InMethod, InTid, InBid, InResult);
	const FString TargetSn = InSn.IsEmpty() ? DockSn : InSn;
	const FString Topic = MakeTopic(kTopicServicesReplyTemplate, TargetSn);
	const FString Json = SerializeJson(Reply);
	if (!Json.IsEmpty())
	{
		FMQTTClientMessage Msg;
		Msg.Topic = Topic;
		Msg.SetPayloadFromString(Json);
		MqttClient->Publish(Msg.Topic, Msg.Payload, EMQTTQualityOfService::Once, false);
		UE_LOG(LogTemp, Log, TEXT("[UAVMqttBridge] 回复 %s -> result=%d"), *InMethod, InResult);
	}
}

void UUAVMqttBridgeComponent::PublishEvent(const FString& InMethod, const TSharedPtr<FJsonObject>& InData, const FString& InTid, const FString& InBid)
{
	if (!MqttClient || !bConnected)
	{
		return;
	}
	const TSharedRef<FJsonObject> Event = MakeEventMessage(InMethod, DockSn, InTid, InBid, InData);
	const FString Topic = MakeTopic(kTopicEventsTemplate, DockSn);
	const FString Json = SerializeJson(Event);
	if (!Json.IsEmpty())
	{
		FMQTTClientMessage Msg;
		Msg.Topic = Topic;
		Msg.SetPayloadFromString(Json);
		MqttClient->Publish(Msg.Topic, Msg.Payload, EMQTTQualityOfService::Once, false);
	}
}

void UUAVMqttBridgeComponent::PublishDeviceState(const FString& InSn, bool bOnline)
{
	if (!MqttClient || !bConnected)
	{
		return;
	}
	const TSharedRef<FJsonObject> State = MakeTelemetryHeader();
	State->SetBoolField(TEXT("online"), bOnline);
	const FString Topic = MakeTopic(kTopicStateTemplate, InSn);
	const FString Json = SerializeJson(State);
	if (!Json.IsEmpty())
	{
		FMQTTClientMessage Msg;
		Msg.Topic = Topic;
		Msg.SetPayloadFromString(Json);
		MqttClient->Publish(Msg.Topic, Msg.Payload, EMQTTQualityOfService::Once, false);
	}
}

void UUAVMqttBridgeComponent::PublishOnlineStatus(bool bOnline)
{
	if (!MqttClient || !bConnected)
	{
		return;
	}
	const TSharedRef<FJsonObject> Status = MakeTelemetryHeader();
	Status->SetBoolField(TEXT("online"), bOnline);
	const FString Topic = MakeTopic(TEXT("sys/product/{sn}/status"), DockSn);
	const FString Json = SerializeJson(Status);
	if (!Json.IsEmpty())
	{
		FMQTTClientMessage Msg;
		Msg.Topic = Topic;
		Msg.SetPayloadFromString(Json);
		MqttClient->Publish(Msg.Topic, Msg.Payload, EMQTTQualityOfService::Once, false);
	}
}

void UUAVMqttBridgeComponent::PublishRaw(const FString& InTopic, const FString& InPayloadJson)
{
	if (!MqttClient || !bConnected)
	{
		return;
	}
	FMQTTClientMessage Msg;
	Msg.Topic = InTopic;
	Msg.SetPayloadFromString(InPayloadJson);
	MqttClient->Publish(Msg.Topic, Msg.Payload, EMQTTQualityOfService::Once, false);
}

// ---- 事件回调：飞控 ----

void UUAVMqttBridgeComponent::OnFlightCommandResult(const FString& InMethod, int32 InResult)
{
	UE_LOG(LogTemp, Log, TEXT("[UAVMqttBridge] 飞控指令 %s 结果=%d"), *InMethod, InResult);
}

void UUAVMqttBridgeComponent::OnTakeoffProgress(const FString& InStatus, const FString& InFlightId, int32 InWayPointIndex, double InRemainingDistance)
{
	const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("result"), 0);
	Data->SetStringField(TEXT("status"), InStatus);
	Data->SetStringField(TEXT("flight_id"), InFlightId);
	Data->SetStringField(TEXT("track_id"), NewUuid());
	Data->SetNumberField(TEXT("way_point_index"), InWayPointIndex);
	Data->SetNumberField(TEXT("remaining_distance"), InRemainingDistance);

	// 剩余时间（秒）：粗略按水平速度估算
	const double Speed = DroneSim ? DroneSim->GetHorizontalSpeed() : 10.0;
	const int32 RemainingTime = Speed > KINDA_SMALL_NUMBER ? static_cast<int32>(InRemainingDistance / Speed) : 0;
	Data->SetNumberField(TEXT("remaining_time"), RemainingTime);
	Data->SetArrayField(TEXT("planned_path_points"), TArray<TSharedPtr<FJsonValue>>());

	PublishEvent(kEventTakeoffToPointProgress, Data);
}

void UUAVMqttBridgeComponent::OnFlighttaskProgress(const FString& InStatus, const FString& InFlightId, int32 InCurrentWaypointIndex, int32 InPercent)
{
	const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("status"), InStatus);
	Data->SetStringField(TEXT("flight_id"), InFlightId);
	Data->SetNumberField(TEXT("currentWaypointIndex"), InCurrentWaypointIndex);
	Data->SetNumberField(TEXT("percent"), InPercent);

	const TSharedRef<FJsonObject> Ext = MakeShared<FJsonObject>();
	Ext->SetStringField(TEXT("flightId"), InFlightId);
	Ext->SetStringField(TEXT("trackId"), NewUuid());
	Ext->SetStringField(TEXT("waylineId"), TEXT("W000000001"));
	Ext->SetNumberField(TEXT("waylineMissionState"), 1);
	Ext->SetNumberField(TEXT("mediaCount"), 0);
	Data->SetObjectField(TEXT("ext"), Ext);

	PublishEvent(kEventFlighttaskProgress, Data);
}

// ---- 事件回调：相机 ----

void UUAVMqttBridgeComponent::OnLiveStatusChanged(const FString& InVideoId)
{
	const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();

	FUAVLiveSession Session;
	bool bStreaming = false;
	int32 Quality = 0;
	FString VideoType = TEXT("zoom");
	if (CameraStream && CameraStream->GetSession(InVideoId, Session))
	{
		bStreaming = Session.bStreaming;
		Quality = Session.VideoQuality;
		VideoType = Session.VideoType;
	}

	Data->SetBoolField(TEXT("status"), bStreaming);
	Data->SetStringField(TEXT("video_id"), InVideoId);
	Data->SetNumberField(TEXT("video_quality"), Quality);
	Data->SetStringField(TEXT("video_type"), VideoType);

	PublishEvent(TEXT("live_status"), Data);
}

void UUAVMqttBridgeComponent::OnLiveCommandResult(const FString& InMethod, int32 InResult)
{
	UE_LOG(LogTemp, Log, TEXT("[UAVMqttBridge] 直播指令 %s 结果=%d"), *InMethod, InResult);
}

// ---- OSD 组装 ----

TSharedPtr<FJsonObject> UUAVMqttBridgeComponent::BuildDroneOsdPayload() const
{
	const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	if (!DroneSim)
	{
		// 未注入模拟组件：返回空 data，安全跳过（6.3）
		return Data;
	}

	const EUAVFlightState State = DroneSim->GetFlightState();
	Data->SetNumberField(TEXT("mode_code"), FlightStateToModeCode(State));

	const FUAVGeoCoordinate Geo = DroneSim->GetCurrentGeoCoordinate();
	Data->SetNumberField(TEXT("latitude"), Geo.Latitude);
	Data->SetNumberField(TEXT("longitude"), Geo.Longitude);
	Data->SetNumberField(TEXT("height"), Geo.Altitude);
	Data->SetNumberField(TEXT("elevation"), Geo.Altitude);

	Data->SetNumberField(TEXT("attitude_head"), DroneSim->GetHeadingDegrees());
	Data->SetNumberField(TEXT("attitude_pitch"), 0.0);
	Data->SetNumberField(TEXT("attitude_roll"), 0.0);
	Data->SetNumberField(TEXT("horizontal_speed"), DroneSim->GetHorizontalSpeed());
	Data->SetNumberField(TEXT("vertical_speed"), DroneSim->GetVerticalSpeed());
	Data->SetNumberField(TEXT("home_distance"), DroneSim->GetRemainingMissionDistance());

	// 电量（双电池，数值由 UAVDroneSim 载荷状态推导，对齐 dock report_drone_osd.py）
	const TSharedRef<FJsonObject> Battery = MakeShared<FJsonObject>();
	{
		TArray<TSharedPtr<FJsonValue>> BatteryCells;
		for (int32 CellIndex = 0; CellIndex < 2; ++CellIndex)
		{
			double TemperatureCelsius = 0.0;
			int32 VoltageMv = 0;
			DroneSim->GetBatteryCell(CellIndex, TemperatureCelsius, VoltageMv);
			const TSharedRef<FJsonObject> Cell = MakeShared<FJsonObject>();
			Cell->SetNumberField(TEXT("index"), CellIndex);
			Cell->SetNumberField(TEXT("temperature"), TemperatureCelsius);
			Cell->SetNumberField(TEXT("voltage"), VoltageMv);
			BatteryCells.Add(MakeShared<FJsonValueObject>(Cell));
		}
		Battery->SetArrayField(TEXT("batteries"), BatteryCells);
	}
	Battery->SetNumberField(TEXT("capacity_percent"), FMath::RoundToInt(DroneSim->GetBatteryCapacityPercent()));
	Battery->SetNumberField(TEXT("landing_power"), DroneSim->GetLandingPowerPercent());
	Battery->SetNumberField(TEXT("remain_flight_time"), DroneSim->GetRemainFlightTimeSeconds());
	Battery->SetNumberField(TEXT("return_home_power"), DroneSim->GetReturnHomePowerPercent());
	Data->SetObjectField(TEXT("battery"), Battery);

	Data->SetStringField(TEXT("firmware_version"), TEXT("12.03.0100"));

	// 载荷（云台角度与变焦，读取 UAVDroneSim 模拟状态）
	const FUAVGimbalState Gimbal = DroneSim->GetGimbalState();
	UUAVDroneSimComponent* Sim = DroneSim;
	const TArray<TSharedPtr<FJsonValue>> Payloads = { MakeShared<FJsonValueObject>(
		[&Gimbal, Sim]() {
			const TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
			P->SetNumberField(TEXT("payload_index"), 0);
			P->SetNumberField(TEXT("gimbal_pitch"), Gimbal.PitchDegrees);
			P->SetNumberField(TEXT("gimbal_roll"), Gimbal.RollDegrees);
			P->SetNumberField(TEXT("gimbal_yaw"), Gimbal.YawDegrees);
			P->SetNumberField(TEXT("zoom_factor"), Sim->GetZoomFactor());
			return P;
		}()) };
	Data->SetArrayField(TEXT("payloads"), Payloads);

	// 摄像头（int 枚举与完整字段，对齐 dock report_drone_osd.py）
	const TSharedRef<FJsonObject> Camera = MakeShared<FJsonObject>();
	Camera->SetNumberField(TEXT("payload_index"), 0);
	Camera->SetNumberField(TEXT("camera_mode"), DroneSim->GetCameraMode());
	Camera->SetNumberField(TEXT("photo_state"), 0);
	Camera->SetNumberField(TEXT("recording_state"), DroneSim->IsRecording() ? 1 : 0);
	Camera->SetNumberField(TEXT("zoom_factor"), DroneSim->GetZoomFactor());
	Camera->SetNumberField(TEXT("ir_zoom_factor"), 2.0);
	Camera->SetNumberField(TEXT("remain_photo_num"), 9999);
	Camera->SetNumberField(TEXT("remain_record_duration"), FMath::Max(0.0, 5400.0 - DroneSim->GetRecordingTimeSeconds()));
	Camera->SetNumberField(TEXT("record_time"), DroneSim->GetRecordingTimeSeconds());
	Camera->SetNumberField(TEXT("zoom_focus_mode"), 0);
	Camera->SetNumberField(TEXT("zoom_focus_value"), 0);
	Camera->SetNumberField(TEXT("zoom_max_focus_value"), 100);
	Camera->SetNumberField(TEXT("zoom_min_focus_value"), 0);
	Camera->SetNumberField(TEXT("zoom_focus_state"), 0);
	Camera->SetBoolField(TEXT("screen_split_enable"), false);
	Camera->SetArrayField(TEXT("photo_storage_settings"), { MakeShared<FJsonValueString>(TEXT("current")) });
	Camera->SetArrayField(TEXT("video_storage_settings"), { MakeShared<FJsonValueString>(TEXT("current")) });
	{
		const TSharedRef<FJsonObject> Region = MakeShared<FJsonObject>();
		Region->SetNumberField(TEXT("left"), 0.0);
		Region->SetNumberField(TEXT("top"), 0.0);
		Region->SetNumberField(TEXT("right"), 1.0);
		Region->SetNumberField(TEXT("bottom"), 1.0);
		Camera->SetObjectField(TEXT("liveview_world_region"), Region);
	}
	Data->SetArrayField(TEXT("cameras"), { MakeShared<FJsonValueObject>(Camera) });

	// ---- 顶层结构补齐（对齐 dock report_drone_osd.py） ----
	Data->SetNumberField(TEXT("gear"), Geo.Altitude > 8.0 ? 1 : 0);
	Data->SetNumberField(TEXT("wind_speed"), 3.0);
	Data->SetNumberField(TEXT("wind_direction"),
		UAVPayloadMath::ComputeWindDirectionEnum(DroneSim->GetHeadingDegrees() + 180.0));
	Data->SetNumberField(TEXT("total_flight_distance"), DroneSim->GetTotalFlightDistanceMeters());
	Data->SetNumberField(TEXT("total_flight_time"), DroneSim->GetTotalFlightTimeSeconds());
	{
		const TSharedRef<FJsonObject> PositionState = MakeShared<FJsonObject>();
		PositionState->SetNumberField(TEXT("gps_number"), 18);
		PositionState->SetNumberField(TEXT("is_fixed"), 2);
		PositionState->SetNumberField(TEXT("quality"), 4);
		PositionState->SetNumberField(TEXT("rtk_number"), 14);
		Data->SetObjectField(TEXT("position_state"), PositionState);
	}
	{
		const TSharedRef<FJsonObject> Speaker = MakeShared<FJsonObject>();
		Speaker->SetNumberField(TEXT("play_volume"), 0);
		Data->SetObjectField(TEXT("speaker"), Speaker);
	}
	Data->SetNumberField(TEXT("speaker_volume"), 0);
	{
		const TSharedRef<FJsonObject> Storage = MakeShared<FJsonObject>();
		Storage->SetNumberField(TEXT("total"), 131072);
		Storage->SetNumberField(TEXT("used"), FMath::Min(131072.0 - 8192.0, DroneSim->GetRecordingTimeSeconds() * 4.2));
		Data->SetObjectField(TEXT("storage"), Storage);
	}
	Data->SetNumberField(TEXT("night_lights_state"), DroneSim->IsRecording() ? 1 : 0);
	Data->SetNumberField(TEXT("height_limit"), 500);
	{
		const TSharedRef<FJsonObject> DistanceLimit = MakeShared<FJsonObject>();
		DistanceLimit->SetNumberField(TEXT("state"), 1);
		DistanceLimit->SetNumberField(TEXT("distance_limit"), 3000);
		DistanceLimit->SetBoolField(TEXT("is_near_distance_limit"), false);
		Data->SetObjectField(TEXT("distance_limit_status"), DistanceLimit);
	}
	{
		const TSharedRef<FJsonObject> Obstacle = MakeShared<FJsonObject>();
		Obstacle->SetNumberField(TEXT("horizon"), 1);
		Obstacle->SetNumberField(TEXT("upside"), 1);
		Obstacle->SetNumberField(TEXT("downside"), 1);
		Data->SetObjectField(TEXT("obstacle_avoidance"), Obstacle);
	}
	Data->SetNumberField(TEXT("activation_time"),
		FDateTime::UtcNow().ToUnixTimestamp() * 1000LL - 86400000LL * 90);
	Data->SetNumberField(TEXT("rc_lost_action"), 2);
	Data->SetNumberField(TEXT("rth_altitude"), 60);
	Data->SetNumberField(TEXT("total_flight_sorties"),
		FMath::Max(1, FMath::FloorToInt(DroneSim->GetTotalFlightTimeSeconds() / 600.0) + 1));
	Data->SetNumberField(TEXT("exit_wayline_when_rc_lost"), 1);
	Data->SetStringField(TEXT("country"), TEXT("CN"));
	Data->SetBoolField(TEXT("rid_state"), false);
	Data->SetBoolField(TEXT("is_near_area_limit"), false);
	Data->SetBoolField(TEXT("is_near_height_limit"), false);
	Data->SetStringField(TEXT("track_id"), FString::Printf(TEXT("SIM-%s"), *DroneSn));

	return Data;
}

int32 UUAVMqttBridgeComponent::FlightStateToModeCode(EUAVFlightState InState) const
{
	switch (InState)
	{
	case EUAVFlightState::Idle:
		return 0;
	case EUAVFlightState::TakingOff:
		return 1;
	case EUAVFlightState::Wayline:
		return 4;
	case EUAVFlightState::Flying:
		return 3;
	case EUAVFlightState::Landing:
		return 10;
	case EUAVFlightState::ReturnHome:
		return 9;
	default:
		return 0;
	}
}

void UUAVMqttBridgeComponent::PublishDroneOsd()
{
	if (!MqttClient || !bConnected || !DroneSim)
	{
		return;
	}
	const TSharedRef<FJsonObject> Osd = MakeTelemetryHeader();
	Osd->SetStringField(TEXT("gateway"), DockSn);
	Osd->SetObjectField(TEXT("data"), BuildDroneOsdPayload());
	const FString Topic = MakeTopic(kTopicOsdTemplate, DroneSn);
	const FString Json = SerializeJson(Osd);
	if (!Json.IsEmpty())
	{
		FMQTTClientMessage Msg;
		Msg.Topic = Topic;
		Msg.SetPayloadFromString(Json);
		MqttClient->Publish(Msg.Topic, Msg.Payload, EMQTTQualityOfService::Once, false);
	}
}

void UUAVMqttBridgeComponent::PublishDockOsd()
{
	if (!MqttClient || !bConnected)
	{
		return;
	}
	const TSharedRef<FJsonObject> Osd = MakeTelemetryHeader();
	Osd->SetStringField(TEXT("gateway"), DockSn);
	const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("sn"), DockSn);
	Data->SetNumberField(TEXT("drone_status"), DroneSim ? static_cast<int32>(DroneSim->GetFlightState()) : 0);
	Osd->SetObjectField(TEXT("data"), Data);
	const FString Topic = MakeTopic(kTopicOsdTemplate, DockSn);
	const FString Json = SerializeJson(Osd);
	if (!Json.IsEmpty())
	{
		FMQTTClientMessage Msg;
		Msg.Topic = Topic;
		Msg.SetPayloadFromString(Json);
		MqttClient->Publish(Msg.Topic, Msg.Payload, EMQTTQualityOfService::Once, false);
	}
}
