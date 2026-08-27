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

	/**
	 * 从 drc/down topic（thing/product/{sn}/drc/down）提取设备 SN。
	 * 提取失败返回空串，由调用方回退默认 SN。
	 */
	FString ParseSnFromDrcTopic(const FString& InTopic)
	{
		const FString Prefix = TEXT("thing/product/");
		const FString Suffix = TEXT("/drc/down");
		if (InTopic.StartsWith(Prefix) && InTopic.EndsWith(Suffix))
		{
			return InTopic.Mid(Prefix.Len(), InTopic.Len() - Prefix.Len() - Suffix.Len());
		}
		return FString();
	}

	/**
	 * flighttask_progress 状态 → WaylineMissionStateEnum（对齐 dock 枚举：5=到达首航点、6=执行中、9=结束）。
	 * 终态（ok/failed/canceled/timeout/partially_done 等）统一映射为 9。
	 */
	int32 WaylineMissionStateFromStatus(const FString& InStatus)
	{
		if (InStatus == TEXT("sent"))
		{
			return 5;
		}
		if (InStatus == TEXT("in_progress"))
		{
			return 6;
		}
		return 9;
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
	// 载荷指令由 UAVCameraStream 处理，注入无人机模拟组件作为载荷状态源
	if (CameraStream)
	{
		CameraStream->SetDroneSim(DroneSim);
	}

	// 绑定飞控/相机事件委托，回调转发为上云 API 事件
	if (FlightControl)
	{
		FlightControl->OnCommandResult.AddDynamic(this, &UUAVMqttBridgeComponent::OnFlightCommandResult);
		FlightControl->OnTakeoffProgress.AddDynamic(this, &UUAVMqttBridgeComponent::OnTakeoffProgress);
		FlightControl->OnFlighttaskProgress.AddDynamic(this, &UUAVMqttBridgeComponent::OnFlighttaskProgress);
		FlightControl->OnReturnHomeStatus.AddDynamic(this, &UUAVMqttBridgeComponent::OnReturnHomeStatus);
		FlightControl->OnFlighttaskReady.AddDynamic(this, &UUAVMqttBridgeComponent::OnFlighttaskReady);
		FlightControl->OnFlyToPointProgress.AddDynamic(this, &UUAVMqttBridgeComponent::OnFlyToPointProgress);
		FlightControl->OnDrcStatusNotify.AddDynamic(this, &UUAVMqttBridgeComponent::OnDrcStatusNotify);
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
		FlightControl->OnReturnHomeStatus.RemoveDynamic(this, &UUAVMqttBridgeComponent::OnReturnHomeStatus);
		FlightControl->OnFlighttaskReady.RemoveDynamic(this, &UUAVMqttBridgeComponent::OnFlighttaskReady);
		FlightControl->OnFlyToPointProgress.RemoveDynamic(this, &UUAVMqttBridgeComponent::OnFlyToPointProgress);
		FlightControl->OnDrcStatusNotify.RemoveDynamic(this, &UUAVMqttBridgeComponent::OnDrcStatusNotify);
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
	DrcSubscription = nullptr;
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

		// 订阅机场 DRC 指令 topic（thing/product/{sn}/drc/down）
		const FString DrcTopic = MakeTopic(kTopicDrcDownTemplate, DockSn);
		DrcSubscription = MqttClient->Subscribe(DrcTopic, EMQTTQualityOfService::Once);
		if (DrcSubscription)
		{
			UMQTTSubscriptionObject::FOnMessageDelegate DrcDelegate;
			DrcDelegate.BindUFunction(this, TEXT("OnDrcMessage"));
			DrcSubscription->SetOnMessageHandler(DrcDelegate);
		}

		// 上报上线状态
		PublishOnlineStatus(true);
		PublishDeviceState(DockSn, true);
		PublishDeviceState(DroneSn, true);
		// 上报直播能力（dock 依赖 live_capacity 建立直播能力缓存）
		PublishLiveCapacity();
		// 上报 HMS 空告警（dock 依赖 hms 记录设备告警）
		PublishHms(BuildHmsPayload());

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

void UUAVMqttBridgeComponent::OnDrcMessage(const FMQTTClientMessage& InMessage)
{
	const FString& PayloadJson = InMessage.GetPayloadAsString();
	if (PayloadJson.IsEmpty())
	{
		return;
	}
	// 提取报文来源设备 SN（topic 形如 thing/product/{sn}/drc/down）
	const FString SourceSn = ParseSnFromDrcTopic(InMessage.Topic);
	DispatchDrcMessage(PayloadJson, SourceSn);
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
		|| Method.StartsWith(TEXT("return_home")) || Method.StartsWith(TEXT("fly_"));
	const bool bIsLiveCommand = Method.StartsWith(TEXT("live_"));
	const bool bIsPayloadCommand = Method.StartsWith(TEXT("camera_")) || Method.StartsWith(TEXT("payload_"))
		|| Method.StartsWith(TEXT("gimbal_")) || Method.StartsWith(TEXT("photo_storage_"))
		|| Method.StartsWith(TEXT("video_storage_")) || Method.StartsWith(TEXT("ir_metering_"))
		|| Method.StartsWith(TEXT("poi_"));

	// DRC 模式指令走 services 通道，精确匹配（其余 drc_* 前缀方法仍按未知指令处理）
	if ((Method == kMethodDrcModeEnter || Method == kMethodDrcModeExit) && FlightControl)
	{
		Result = FlightControl->HandleCommand(Method, DataJson);
	}
	else if (bIsFlightCommand && FlightControl)
	{
		Result = FlightControl->HandleCommand(Method, DataJson);
	}
	else if (bIsLiveCommand && CameraStream)
	{
		Result = CameraStream->HandleCommand(Method, DataJson);
	}
	else if (bIsPayloadCommand && CameraStream)
	{
		Result = CameraStream->HandleCommand(Method, DataJson);
		// 载荷权抢占成功后补发载荷控制源 state（对齐 dock report_control_source.py）
		if (Method == kMethodPayloadAuthorityGrab && Result == UAV::FlightControlResult::Success)
		{
			PublishPayloadControlSource();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[UAVMqttBridge] 未知或未支持的指令: %s"), *Method);
	}

	// 回发 services_reply（按来源设备 SN，缺省回退机场 SN）
	PublishServicesReply(Method, Tid, Bid, Result, InSn);
	OnServiceCommandReceived.Broadcast(Method);
}

void UUAVMqttBridgeComponent::DispatchDrcMessage(const FString& InPayloadJson, const FString& InSn)
{
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(InPayloadJson), Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[UAVMqttBridge] drc/down 报文解析失败: %s"), *InPayloadJson);
		return;
	}

	const FString Tid = Root->GetStringField(TEXT("tid"));
	const FString Bid = Root->GetStringField(TEXT("bid"));
	const FString Method = Root->GetStringField(TEXT("method"));
	if (Method.IsEmpty())
	{
		return;
	}

	// 取 data 字段并序列化为 JSON 字符串传给飞控
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
	if (FlightControl)
	{
		Result = FlightControl->HandleCommand(Method, DataJson);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[UAVMqttBridge] 飞控未注入，无法处理 DRC 指令: %s"), *Method);
	}

	// 回发 drc/up 回执（按来源设备 SN，缺省回退机场 SN）
	PublishDrcUpReply(Method, Tid, Bid, Result, InSn);
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

void UUAVMqttBridgeComponent::PublishDrcUpReply(const FString& InMethod, const FString& InTid, const FString& InBid, int32 InResult, const FString& InSn)
{
	if (!MqttClient || !bConnected)
	{
		return;
	}
	// drone_control / heart_beat 回执携带最近 seq；drone_emergency_stop 仅 result
	const bool bIncludeSeq = (InMethod == kMethodDroneControl || InMethod == kMethodHeartBeat);
	const int32 Seq = (FlightControl && bIncludeSeq) ? FlightControl->GetLastDrcSeq() : -1;
	const TSharedRef<FJsonObject> Reply = BuildDrcUpReply(InMethod, InTid, InBid, InResult, Seq).ToSharedRef();
	const FString TargetSn = InSn.IsEmpty() ? DockSn : InSn;
	const FString Topic = MakeTopic(kTopicDrcUpTemplate, TargetSn);
	const FString Json = SerializeJson(Reply);
	if (!Json.IsEmpty())
	{
		FMQTTClientMessage Msg;
		Msg.Topic = Topic;
		Msg.SetPayloadFromString(Json);
		MqttClient->Publish(Msg.Topic, Msg.Payload, EMQTTQualityOfService::Once, false);
		UE_LOG(LogTemp, Log, TEXT("[UAVMqttBridge] DRC 回执 %s -> result=%d"), *InMethod, InResult);
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

void UUAVMqttBridgeComponent::PublishPayloadControlSource()
{
	if (!MqttClient || !bConnected)
	{
		return;
	}
	const TSharedRef<FJsonObject> State = MakeTelemetryHeader();
	State->SetStringField(TEXT("gateway"), DockSn);
	const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("control_source"), TEXT("A"));
	const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("control_source"), TEXT("A"));
	Payload->SetStringField(TEXT("payload_index"), CameraIndex);
	Data->SetArrayField(TEXT("payloads"), { MakeShared<FJsonValueObject>(Payload) });
	State->SetObjectField(TEXT("data"), Data);

	const FString Topic = MakeTopic(kTopicStateTemplate, DroneSn);
	const FString Json = SerializeJson(State);
	if (!Json.IsEmpty())
	{
		FMQTTClientMessage Msg;
		Msg.Topic = Topic;
		Msg.SetPayloadFromString(Json);
		MqttClient->Publish(Msg.Topic, Msg.Payload, EMQTTQualityOfService::Once, false);
		UE_LOG(LogTemp, Log, TEXT("[UAVMqttBridge] 发布载荷控制源 state：%s"), *Json);
	}
}

void UUAVMqttBridgeComponent::PublishLiveCapacity()
{
	if (!MqttClient || !bConnected)
	{
		return;
	}
	const TSharedRef<FJsonObject> State = MakeTelemetryHeader();
	State->SetStringField(TEXT("gateway"), DockSn);
	State->SetObjectField(TEXT("data"), BuildLiveCapacityPayload().ToSharedRef());

	const FString Topic = MakeTopic(kTopicStateTemplate, DockSn);
	const FString Json = SerializeJson(State);
	if (!Json.IsEmpty())
	{
		FMQTTClientMessage Msg;
		Msg.Topic = Topic;
		Msg.SetPayloadFromString(Json);
		MqttClient->Publish(Msg.Topic, Msg.Payload, EMQTTQualityOfService::Once, false);
		UE_LOG(LogTemp, Log, TEXT("[UAVMqttBridge] 发布直播能力 state：%s"), *Json);
	}
}

void UUAVMqttBridgeComponent::PublishHms(const TSharedPtr<FJsonObject>& InHmsData)
{
	if (!MqttClient || !bConnected || !InHmsData.IsValid())
	{
		return;
	}
	PublishEvent(kEventHms, InHmsData);
	UE_LOG(LogTemp, Log, TEXT("[UAVMqttBridge] 发布 HMS 告警事件：%s"), *SerializeJson(InHmsData.ToSharedRef()));
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
	PublishEvent(kEventFlighttaskProgress, BuildFlighttaskProgressEventData(InStatus, InFlightId, InCurrentWaypointIndex, InPercent));
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

void UUAVMqttBridgeComponent::OnReturnHomeStatus(const FString& InStatus, const FString& InReason)
{
	// 对齐 dock EventsMethodEnum：返航状态事件为 return_home_info（不再发布 return_home_status）
	PublishEvent(kEventReturnHomeInfo, BuildReturnHomeInfoEventData());
	// 低电量自动返航连带上报 HMS 告警（dock DeviceHmsServiceImpl.hms）
	if (InStatus == TEXT("rth_auto_trigger") && InReason == TEXT("battery_low"))
	{
		PublishHms(BuildHmsPayload(true));
	}
	UE_LOG(LogTemp, Log, TEXT("[UAVMqttBridge] 发布返航状态事件：status=%s reason=%s"), *InStatus, *InReason);
}

void UUAVMqttBridgeComponent::OnFlighttaskReady(const FString& InFlightId)
{
	PublishEvent(kEventFlighttaskReady, BuildFlighttaskReadyData(InFlightId));
	UE_LOG(LogTemp, Log, TEXT("[UAVMqttBridge] 发布任务就绪事件：flight_id=%s"), *InFlightId);
}

void UUAVMqttBridgeComponent::OnFlyToPointProgress(const FString& InStatus, const FString& InFlyToId, int32 InWayPointIndex, int32 InResult)
{
	PublishEvent(kEventFlyToPointProgress, BuildFlyToPointProgressEventData(InStatus, InFlyToId, InWayPointIndex, InResult));
	UE_LOG(LogTemp, Log, TEXT("[UAVMqttBridge] 发布指点飞行进度事件：status=%s fly_to_id=%s way_point_index=%d result=%d"), *InStatus, *InFlyToId, InWayPointIndex, InResult);
}

void UUAVMqttBridgeComponent::OnDrcStatusNotify(int32 InDrcState)
{
	PublishEvent(kEventDrcStatusNotify, BuildDrcStatusNotifyData(InDrcState));
	UE_LOG(LogTemp, Log, TEXT("[UAVMqttBridge] 发布 DRC 状态事件：drc_state=%d"), InDrcState);
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
	Camera->SetNumberField(TEXT("photo_state"), DroneSim->IsPhotoTaking() ? 1 : 0);
	Camera->SetNumberField(TEXT("recording_state"), DroneSim->IsRecording() ? 1 : 0);
	Camera->SetNumberField(TEXT("zoom_factor"), DroneSim->GetZoomFactor());
	Camera->SetNumberField(TEXT("ir_zoom_factor"), 2.0);
	Camera->SetNumberField(TEXT("remain_photo_num"), DroneSim->GetRemainingPhotoNum());
	Camera->SetNumberField(TEXT("remain_record_duration"), FMath::Max(0.0, 5400.0 - DroneSim->GetRecordingTimeSeconds()));
	Camera->SetNumberField(TEXT("record_time"), DroneSim->GetRecordingTimeSeconds());
	Camera->SetNumberField(TEXT("zoom_focus_mode"), DroneSim->GetFocusMode());
	Camera->SetNumberField(TEXT("zoom_focus_value"), DroneSim->GetFocusValue());
	Camera->SetNumberField(TEXT("zoom_max_focus_value"), 100);
	Camera->SetNumberField(TEXT("zoom_min_focus_value"), 0);
	Camera->SetNumberField(TEXT("zoom_focus_state"), 0);
	Camera->SetBoolField(TEXT("screen_split_enable"), DroneSim->IsScreenSplitEnabled());
	Camera->SetArrayField(TEXT("photo_storage_settings"), { MakeShared<FJsonValueString>(DroneSim->GetPhotoStorageLocation()) });
	Camera->SetArrayField(TEXT("video_storage_settings"), { MakeShared<FJsonValueString>(DroneSim->GetVideoStorageLocation()) });
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
	Osd->SetObjectField(TEXT("data"), BuildDockOsdPayload().ToSharedRef());
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

bool UUAVMqttBridgeComponent::IsDroneInDock() const
{
	if (!DroneSim)
	{
		return false;
	}
	// 对齐 dock report_dock_osd.py：无人机在机场原点 ±0.00002 度内、高度 ≤12、且不在任务中
	const FUAVGeoCoordinate Geo = DroneSim->GetCurrentGeoCoordinate();
	const bool bNearAirport = FMath::Abs(Geo.Latitude - DroneSim->AirportOrigin.Latitude) < 0.00002
		&& FMath::Abs(Geo.Longitude - DroneSim->AirportOrigin.Longitude) < 0.00002;
	return bNearAirport && Geo.Altitude <= 12.0 && DroneSim->GetFlightState() == EUAVFlightState::Idle;
}

int32 UUAVMqttBridgeComponent::GetFlightTaskStepCode() const
{
	if (!DroneSim)
	{
		return 5;
	}
	// 对齐 dock 口径：任务中（起飞/航线）=0，返航/降落=2，其余=5
	switch (DroneSim->GetFlightState())
	{
	case EUAVFlightState::TakingOff:
	case EUAVFlightState::Wayline:
		return 0;
	case EUAVFlightState::ReturnHome:
	case EUAVFlightState::Landing:
		return 2;
	default:
		return 5;
	}
}

bool UUAVMqttBridgeComponent::IsDockInMission() const
{
	return DroneSim && DroneSim->GetFlightState() != EUAVFlightState::Idle;
}

TSharedPtr<FJsonObject> UUAVMqttBridgeComponent::BuildLiveCapacityPayload() const
{
	// 对齐 dock report_live_capacity.py：网关（165-0-7 普通相机）+ 无人机（176-0-0 普通相机、52-0-0 主载荷）
	const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	const TSharedRef<FJsonObject> Capacity = MakeShared<FJsonObject>();

	// 构造视频项（video_index/video_type/switchable_video_types）
	auto MakeVideo = [](const FString& InVideoIndex, const FString& InVideoType, const TArray<FString>& InSwitchableTypes)
	{
		const TSharedRef<FJsonObject> Video = MakeShared<FJsonObject>();
		Video->SetStringField(TEXT("video_index"), InVideoIndex);
		Video->SetStringField(TEXT("video_type"), InVideoType);
		TArray<TSharedPtr<FJsonValue>> Switchable;
		for (const FString& Type : InSwitchableTypes)
		{
			Switchable.Add(MakeShared<FJsonValueString>(Type));
		}
		Video->SetArrayField(TEXT("switchable_video_types"), Switchable);
		return Video;
	};

	// 构造相机项（camera_index/available_video_number/coexist_video_number_max/video_list）
	auto MakeCamera = [&MakeVideo](const FString& InCameraIndex, const TArray<TSharedPtr<FJsonValue>>& InVideoList)
	{
		const TSharedRef<FJsonObject> Camera = MakeShared<FJsonObject>();
		Camera->SetStringField(TEXT("camera_index"), InCameraIndex);
		Camera->SetNumberField(TEXT("available_video_number"), InVideoList.Num());
		Camera->SetNumberField(TEXT("coexist_video_number_max"), 1);
		Camera->SetArrayField(TEXT("video_list"), InVideoList);
		return Camera;
	};

	// 网关直播设备项：机场 165-0-7 相机（normal-0 / normal）
	const TArray<TSharedPtr<FJsonValue>> GatewayVideos = {
		MakeShared<FJsonValueObject>(MakeVideo(TEXT("normal-0"), TEXT("normal"), { TEXT("normal") })),
	};
	const TSharedRef<FJsonObject> GatewayDevice = MakeShared<FJsonObject>();
	GatewayDevice->SetStringField(TEXT("sn"), DockSn);
	GatewayDevice->SetNumberField(TEXT("available_video_number"), GatewayVideos.Num());
	GatewayDevice->SetNumberField(TEXT("coexist_video_number_max"), 1);
	GatewayDevice->SetArrayField(TEXT("camera_list"), {
		MakeShared<FJsonValueObject>(MakeCamera(TEXT("165-0-7"), GatewayVideos)),
	});

	// 无人机直播设备项：176-0-0 普通相机（normal）+ 相机索引主载荷（zoom，可切换 normal/wide/zoom/ir）
	const TArray<TSharedPtr<FJsonValue>> NormalVideos = {
		MakeShared<FJsonValueObject>(MakeVideo(TEXT("normal-0"), TEXT("normal"), { TEXT("normal") })),
	};
	const TArray<TSharedPtr<FJsonValue>> PayloadVideos = {
		MakeShared<FJsonValueObject>(MakeVideo(TEXT("normal-0"), TEXT("zoom"), { TEXT("normal"), TEXT("wide"), TEXT("zoom"), TEXT("ir") })),
	};
	const TSharedRef<FJsonObject> DroneDevice = MakeShared<FJsonObject>();
	DroneDevice->SetStringField(TEXT("sn"), DroneSn);
	DroneDevice->SetNumberField(TEXT("available_video_number"), NormalVideos.Num() + PayloadVideos.Num());
	DroneDevice->SetNumberField(TEXT("coexist_video_number_max"), 1);
	DroneDevice->SetArrayField(TEXT("camera_list"), {
		MakeShared<FJsonValueObject>(MakeCamera(TEXT("176-0-0"), NormalVideos)),
		MakeShared<FJsonValueObject>(MakeCamera(CameraIndex, PayloadVideos)),
	});

	const TArray<TSharedPtr<FJsonValue>> DeviceList = {
		MakeShared<FJsonValueObject>(GatewayDevice),
		MakeShared<FJsonValueObject>(DroneDevice),
	};
	Capacity->SetNumberField(TEXT("available_video_number"), 3);
	Capacity->SetNumberField(TEXT("coexist_video_number_max"), 3);
	Capacity->SetArrayField(TEXT("device_list"), DeviceList);
	Data->SetObjectField(TEXT("live_capacity"), Capacity);
	return Data;
}

TSharedPtr<FJsonObject> UUAVMqttBridgeComponent::BuildFlighttaskProgressEventData(const FString& InStatus, const FString& InFlightId, int32 InCurrentWaypointIndex, int32 InPercent) const
{
	// 对齐 dock EventsDataRequest<FlighttaskProgress>：data = { result, output:{ status, progress, ext } }
	const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("result"), 0);

	const TSharedRef<FJsonObject> Output = MakeShared<FJsonObject>();
	Output->SetStringField(TEXT("status"), InStatus);

	const TSharedRef<FJsonObject> Progress = MakeShared<FJsonObject>();
	Progress->SetNumberField(TEXT("current_step"), InCurrentWaypointIndex);
	Progress->SetNumberField(TEXT("percent"), InPercent);
	Output->SetObjectField(TEXT("progress"), Progress);

	const TSharedRef<FJsonObject> Ext = MakeShared<FJsonObject>();
	Ext->SetNumberField(TEXT("current_waypoint_index"), InCurrentWaypointIndex);
	Ext->SetNumberField(TEXT("media_count"), 0);
	Ext->SetStringField(TEXT("flight_id"), InFlightId);
	Ext->SetStringField(TEXT("track_id"), NewUuid());
	Ext->SetStringField(TEXT("wayline_id"), TEXT("W000000001"));
	Ext->SetNumberField(TEXT("wayline_mission_state"), WaylineMissionStateFromStatus(InStatus));
	Output->SetObjectField(TEXT("ext"), Ext);

	Data->SetObjectField(TEXT("output"), Output);
	return Data;
}

TSharedPtr<FJsonObject> UUAVMqttBridgeComponent::BuildFlyToPointProgressEventData(const FString& InStatus, const FString& InFlyToId, int32 InWayPointIndex, int32 InResult) const
{
	// 对齐 dock FlyToPointProgress：data = { result, status, fly_to_id, way_point_index }
	const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("result"), InResult);
	Data->SetStringField(TEXT("status"), InStatus);
	Data->SetStringField(TEXT("fly_to_id"), InFlyToId);
	Data->SetNumberField(TEXT("way_point_index"), InWayPointIndex);
	return Data;
}

TSharedPtr<FJsonObject> UUAVMqttBridgeComponent::BuildReturnHomeInfoEventData() const
{
	// 对齐 dock ReturnHomeInfo：planned_path_points / last_point_type / flight_id
	const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> PathPoints;
	if (DroneSim)
	{
		const TSharedRef<FJsonObject> Point = MakeShared<FJsonObject>();
		Point->SetNumberField(TEXT("latitude"), DroneSim->AirportOrigin.Latitude);
		Point->SetNumberField(TEXT("longitude"), DroneSim->AirportOrigin.Longitude);
		Point->SetNumberField(TEXT("height"), FlightControl ? FlightControl->GetCurrentRthAltitude() : 0.0);
		PathPoints.Add(MakeShared<FJsonValueObject>(Point));
	}
	Data->SetArrayField(TEXT("planned_path_points"), PathPoints);
	Data->SetNumberField(TEXT("last_point_type"), PathPoints.Num() > 0 ? 0 : 65535);
	Data->SetStringField(TEXT("flight_id"), FlightControl ? FlightControl->GetCurrentFlightId() : FString());
	return Data;
}

TSharedPtr<FJsonObject> UUAVMqttBridgeComponent::BuildFlighttaskReadyData(const FString& InFlightId) const
{
	// 对齐 dock FlightTaskServiceImpl.flighttaskReady：data.flight_ids
	const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> FlightIds;
	FlightIds.Add(MakeShared<FJsonValueString>(InFlightId));
	Data->SetArrayField(TEXT("flight_ids"), FlightIds);
	return Data;
}

TSharedPtr<FJsonObject> UUAVMqttBridgeComponent::BuildHmsPayload(bool bLowBatteryAlarm) const
{
	// 对齐 dock Hms / DeviceHms / DeviceHmsArgs（Jackson snake_case）
	const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> List;
	if (bLowBatteryAlarm)
	{
		const TSharedRef<FJsonObject> Alarm = MakeShared<FJsonObject>();
		Alarm->SetStringField(TEXT("code"), TEXT("fpv_tip_0x1B030014"));
		Alarm->SetStringField(TEXT("device_type"), TEXT("0-100-1"));
		Alarm->SetBoolField(TEXT("imminent"), true);
		Alarm->SetBoolField(TEXT("in_the_sky"), true);
		Alarm->SetNumberField(TEXT("level"), 1);
		Alarm->SetNumberField(TEXT("module"), 0);
		const TSharedRef<FJsonObject> Args = MakeShared<FJsonObject>();
		Args->SetNumberField(TEXT("component_index"), 0);
		Alarm->SetObjectField(TEXT("args"), Args);
		List.Add(MakeShared<FJsonValueObject>(Alarm));
	}
	Data->SetArrayField(TEXT("list"), List);
	return Data;
}

TSharedPtr<FJsonObject> UUAVMqttBridgeComponent::BuildDrcUpReply(const FString& InMethod, const FString& InTid, const FString& InBid, int32 InResult, int32 InSeq) const
{
	// 对齐 dock DrcUpData：{tid, bid, timestamp, method, data:{result, output?:{seq}}}，复用 services_reply 结构
	TSharedPtr<FJsonObject> Output;
	if (InSeq >= 0)
	{
		Output = MakeShared<FJsonObject>();
		Output->SetNumberField(TEXT("seq"), InSeq);
	}
	return MakeServicesReply(InMethod, InTid, InBid, InResult, Output);
}

TSharedPtr<FJsonObject> UUAVMqttBridgeComponent::BuildDrcStatusNotifyData(int32 InDrcState) const
{
	// 对齐 dock EventsDataRequest<DrcState>：data={result:0, drc_state}
	const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("result"), 0);
	Data->SetNumberField(TEXT("drc_state"), InDrcState);
	return Data;
}

TSharedPtr<FJsonObject> UUAVMqttBridgeComponent::BuildDockOsdPayload() const
{
	const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	if (!DroneSim)
	{
		// 未注入模拟组件：返回空 data，安全跳过（与无人机 OSD 一致）
		return Data;
	}

	const bool bInDock = IsDroneInDock();
	const bool bCharging = bInDock && DroneSim->GetBatteryCapacityPercent() < 100.0;
	const int32 CapacityPercent = FMath::RoundToInt(DroneSim->GetBatteryCapacityPercent());
	const int32 NowMillis = static_cast<int32>(FDateTime::UtcNow().ToUnixTimestamp() * 1000LL);
	const FUAVGeoCoordinate Airport = DroneSim->AirportOrigin;

	Data->SetStringField(TEXT("sn"), DockSn);
	Data->SetNumberField(TEXT("mode_code"), IsDockInMission() ? 4 : 3);
	Data->SetBoolField(TEXT("drone_in_dock"), bInDock);
	Data->SetNumberField(TEXT("cover_state"), bInDock ? 0 : 1);
	{
		// 充电状态：归巢待命且电量未满为充电中（dock 口径）
		const TSharedRef<FJsonObject> Charge = MakeShared<FJsonObject>();
		Charge->SetNumberField(TEXT("capacity_percent"), CapacityPercent);
		Charge->SetNumberField(TEXT("state"), bCharging ? 1 : 0);
		Data->SetObjectField(TEXT("drone_charge_state"), Charge);
	}
	Data->SetNumberField(TEXT("flighttask_step_code"), GetFlightTaskStepCode());
	Data->SetNumberField(TEXT("flighttask_prepare_capacity"), CapacityPercent);
	Data->SetNumberField(TEXT("acc_time"), FMath::RoundToInt(DroneSim->GetTotalFlightTimeSeconds()));
	Data->SetNumberField(TEXT("job_number"), 0);
	Data->SetNumberField(TEXT("rainfall"), 0);
	Data->SetNumberField(TEXT("wind_speed"), 3.0);
	Data->SetNumberField(TEXT("environment_temperature"), 22.5);
	Data->SetNumberField(TEXT("temperature"), 24.0);
	Data->SetNumberField(TEXT("humidity"), 58);
	Data->SetNumberField(TEXT("latitude"), Airport.Latitude);
	Data->SetNumberField(TEXT("longitude"), Airport.Longitude);
	Data->SetNumberField(TEXT("height"), 12.0);
	{
		// 备降点：机场附近偏移点（dock 基线）
		const TSharedRef<FJsonObject> Alternate = MakeShared<FJsonObject>();
		Alternate->SetNumberField(TEXT("latitude"), Airport.Latitude + 0.00012);
		Alternate->SetNumberField(TEXT("longitude"), Airport.Longitude + 0.00010);
		Alternate->SetNumberField(TEXT("safe_land_height"), 30.0);
		Alternate->SetBoolField(TEXT("is_configured"), true);
		Data->SetObjectField(TEXT("alternate_land_point"), Alternate);
	}
	Data->SetNumberField(TEXT("first_power_on"), NowMillis - 86400000LL * 180);
	Data->SetNumberField(TEXT("activation_time"), NowMillis - 86400000LL * 120);
	{
		// 维护状态：新机场未保养基线（对齐 OsdDockMaintainStatus / DockMaintainStatus）
		const TSharedRef<FJsonObject> Maintain = MakeShared<FJsonObject>();
		const TSharedRef<FJsonObject> MaintainItem = MakeShared<FJsonObject>();
		MaintainItem->SetNumberField(TEXT("last_maintain_flight_sorties"), 0);
		MaintainItem->SetNumberField(TEXT("last_maintain_time"), 0);
		MaintainItem->SetNumberField(TEXT("last_maintain_type"), 0); // 0=NO 未保养
		MaintainItem->SetBoolField(TEXT("state"), false);
		TArray<TSharedPtr<FJsonValue>> MaintainArray;
		MaintainArray.Add(MakeShared<FJsonValueObject>(MaintainItem));
		Maintain->SetArrayField(TEXT("maintain_status_array"), MaintainArray);
		Data->SetObjectField(TEXT("maintain_status"), Maintain);
	}
	{
		const TSharedRef<FJsonObject> Position = MakeShared<FJsonObject>();
		Position->SetBoolField(TEXT("is_calibration"), false);
		Position->SetNumberField(TEXT("gps_number"), 21);
		Position->SetNumberField(TEXT("is_fixed"), 2);
		Position->SetNumberField(TEXT("quality"), 5);
		Position->SetNumberField(TEXT("rtk_number"), 17);
		Data->SetObjectField(TEXT("position_state"), Position);
	}
	{
		// 机库存储：total=512GB（MB），used 随录制时长递增
		const TSharedRef<FJsonObject> Storage = MakeShared<FJsonObject>();
		Storage->SetNumberField(TEXT("total"), 512 * 1024);
		Storage->SetNumberField(TEXT("used"), FMath::Min(512.0 * 1024.0, 64000.0 + DroneSim->GetRecordingTimeSeconds() * 4.2));
		Data->SetObjectField(TEXT("storage"), Storage);
	}
	Data->SetBoolField(TEXT("supplement_light_state"), false);
	Data->SetBoolField(TEXT("emergency_stop_state"), false);
	{
		const TSharedRef<FJsonObject> AirConditioner = MakeShared<FJsonObject>();
		AirConditioner->SetNumberField(TEXT("air_conditioner_state"), 0);
		AirConditioner->SetNumberField(TEXT("switch_time"), 0);
		Data->SetObjectField(TEXT("air_conditioner"), AirConditioner);
	}
	Data->SetNumberField(TEXT("battery_store_mode"), 1);
	Data->SetBoolField(TEXT("alarm_state"), false);
	Data->SetNumberField(TEXT("putter_state"), 0);
	Data->SetNumberField(TEXT("electric_supply_voltage"), 220);
	Data->SetNumberField(TEXT("working_voltage"), 24);
	Data->SetNumberField(TEXT("working_current"), 3);
	{
		const TSharedRef<FJsonObject> Backup = MakeShared<FJsonObject>();
		Backup->SetNumberField(TEXT("voltage"), 24000);
		Backup->SetNumberField(TEXT("temperature"), 29.5);
		Backup->SetBoolField(TEXT("switch"), true);
		Data->SetObjectField(TEXT("backup_battery"), Backup);
	}
	{
		const TSharedRef<FJsonObject> Maintenance = MakeShared<FJsonObject>();
		Maintenance->SetNumberField(TEXT("maintenance_state"), 0);
		Maintenance->SetNumberField(TEXT("maintenance_time_left"), 23);
		Maintenance->SetNumberField(TEXT("heat_state"), 0);
		Data->SetObjectField(TEXT("drone_battery_maintenance_info"), Maintenance);
	}
	{
		const TSharedRef<FJsonObject> Media = MakeShared<FJsonObject>();
		Media->SetNumberField(TEXT("remain_upload"), 0);
		Data->SetObjectField(TEXT("media_file_detail"), Media);
	}
	{
		// 子设备：对接无人机（M4TD）
		const TSharedRef<FJsonObject> SubDevice = MakeShared<FJsonObject>();
		SubDevice->SetStringField(TEXT("device_sn"), DroneSn);
		SubDevice->SetStringField(TEXT("device_model_key"), TEXT("0-100-0"));
		SubDevice->SetNumberField(TEXT("device_online_status"), 1);
		SubDevice->SetNumberField(TEXT("device_paired"), 1);
		Data->SetObjectField(TEXT("sub_device"), SubDevice);
	}
	{
		// 网络状态：4G 在线全质量（dock 基线场景）
		const TSharedRef<FJsonObject> Network = MakeShared<FJsonObject>();
		Network->SetNumberField(TEXT("type"), 2);
		Network->SetNumberField(TEXT("quality"), 5);
		Network->SetNumberField(TEXT("rate"), 100.0);
		Data->SetObjectField(TEXT("network_state"), Network);
	}
	{
		// 图传链路：4G/SDR 双链路在线（dock 基线场景）
		const TSharedRef<FJsonObject> Link = MakeShared<FJsonObject>();
		Link->SetNumberField(TEXT("4g_freq_band"), 2.6);
		Link->SetNumberField(TEXT("4g_gnd_quality"), 5);
		Link->SetNumberField(TEXT("4g_link_state"), 1);
		Link->SetNumberField(TEXT("4g_quality"), 5);
		Link->SetNumberField(TEXT("4g_uav_quality"), 5);
		Link->SetNumberField(TEXT("dongle_number"), 1);
		Link->SetNumberField(TEXT("link_workmode"), 1);
		Link->SetNumberField(TEXT("sdr_freq_band"), 5.8);
		Link->SetNumberField(TEXT("sdr_link_state"), 1);
		Link->SetNumberField(TEXT("sdr_quality"), 5);
		Data->SetObjectField(TEXT("wireless_link"), Link);
	}
	Data->SetNumberField(TEXT("drc_state"), 0);
	Data->SetNumberField(TEXT("user_experience_improvement"), 2);

	return Data;
}
