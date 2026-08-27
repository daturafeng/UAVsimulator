// 上云 API 协议常量与报文工具实现
#include "UAVCloudApiTypes.h"

#include "Misc/DateTime.h"
#include "Misc/Guid.h"

namespace UAV::CloudApi
{
	// ---- MQTT Topic 模板 ----
	const TCHAR* kTopicServicesTemplate = TEXT("thing/product/{sn}/services");
	const TCHAR* kTopicServicesReplyTemplate = TEXT("thing/product/{sn}/services_reply");
	const TCHAR* kTopicEventsTemplate = TEXT("thing/product/{sn}/events");
	const TCHAR* kTopicOsdTemplate = TEXT("thing/product/{sn}/osd");
	const TCHAR* kTopicStateTemplate = TEXT("thing/product/{sn}/state");

	// ---- 服务指令 method ----
	const TCHAR* kMethodFlightAuthorityGrab = TEXT("flight_authority_grab");
	const TCHAR* kMethodTakeoffToPoint = TEXT("takeoff_to_point");
	const TCHAR* kMethodFlighttaskCreate = TEXT("flighttask_create");
	const TCHAR* kMethodFlighttaskPrepare = TEXT("flighttask_prepare");
	const TCHAR* kMethodFlighttaskExecute = TEXT("flighttask_execute");
	const TCHAR* kMethodFlighttaskUndo = TEXT("flighttask_undo");
	const TCHAR* kMethodFlighttaskPause = TEXT("flighttask_pause");
	const TCHAR* kMethodFlighttaskRecovery = TEXT("flighttask_recovery");
	const TCHAR* kMethodReturnHome = TEXT("return_home");
	const TCHAR* kMethodReturnHomeCancel = TEXT("return_home_cancel");
	const TCHAR* kMethodLiveStartPush = TEXT("live_start_push");
	const TCHAR* kMethodLiveStopPush = TEXT("live_stop_push");
	const TCHAR* kMethodLiveSetQuality = TEXT("live_set_quality");
	const TCHAR* kMethodLiveLensChange = TEXT("live_lens_change");

	// ---- 默认设备标识（与 dock script/common.py 假设备口径一致）----
	const TCHAR* kDefaultDockSn = TEXT("DOCK3TEST001");
	const TCHAR* kDefaultDroneSn = TEXT("1581F8HGXTEST001");
	const TCHAR* kDefaultCameraIndex = TEXT("52-0-0");
	const TCHAR* kDefaultVideoType = TEXT("zoom");

	// ---- 事件 method ----
	const TCHAR* kEventTakeoffToPointProgress = TEXT("takeoff_to_point_progress");
	const TCHAR* kEventFlighttaskProgress = TEXT("flighttask_progress");

	FString MakeTopic(const FString& InTemplate, const FString& InSn)
	{
		return InTemplate.Replace(TEXT("{sn}"), *InSn);
	}

	FString MakeVideoId(const FString& InDroneSn, const FString& InCameraIndex, const FString& InVideoType)
	{
		// 格式：{droneSn}/{cameraIndex}/{videoType}-0（与 dock 组装视频ID 一致）
		return FString::Printf(TEXT("%s/%s/%s-0"), *InDroneSn, *InCameraIndex, *InVideoType);
	}

	FString MakeRtmpPushUrl(const FString& InRtmpBaseUrl, const FString& InDroneSn, const FString& InCameraIndex)
	{
		// 格式：rtmpBaseUrl + {droneSn}-{cameraIndex}
		FString BaseUrl = InRtmpBaseUrl;
		if (!BaseUrl.IsEmpty() && !BaseUrl.EndsWith(TEXT("/")))
		{
			BaseUrl += TEXT("/");
		}
		return FString::Printf(TEXT("%s%s-%s"), *BaseUrl, *InDroneSn, *InCameraIndex);
	}

	FString NewUuid()
	{
		return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	}

	int64 NowTimestampMs()
	{
		const FDateTime Now = FDateTime::UtcNow();
		return Now.ToUnixTimestamp() * 1000LL + Now.GetMillisecond();
	}

	TSharedRef<FJsonObject> MakeMessageHeader(const FString& InMethod, const FString& InBid, const TSharedPtr<FJsonObject>& InData)
	{
		TSharedRef<FJsonObject> Header = MakeShared<FJsonObject>();
		Header->SetStringField(TEXT("tid"), NewUuid());
		Header->SetStringField(TEXT("bid"), InBid.IsEmpty() ? NewUuid() : InBid);
		Header->SetNumberField(TEXT("timestamp"), static_cast<double>(NowTimestampMs()));
		Header->SetStringField(TEXT("method"), InMethod);
		if (InData.IsValid())
		{
			Header->SetObjectField(TEXT("data"), InData);
		}
		return Header;
	}

	TSharedRef<FJsonObject> MakeServicesReply(const FString& InMethod, const FString& InTid, const FString& InBid, int32 InResult, const TSharedPtr<FJsonObject>& InOutput)
	{
		TSharedRef<FJsonObject> Reply = MakeShared<FJsonObject>();
		Reply->SetStringField(TEXT("tid"), InTid.IsEmpty() ? NewUuid() : InTid);
		Reply->SetStringField(TEXT("bid"), InBid.IsEmpty() ? NewUuid() : InBid);
		Reply->SetStringField(TEXT("method"), InMethod);
		Reply->SetNumberField(TEXT("timestamp"), static_cast<double>(NowTimestampMs()));

		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetNumberField(TEXT("result"), InResult);
		if (InOutput.IsValid())
		{
			Data->SetObjectField(TEXT("output"), InOutput);
		}
		Reply->SetObjectField(TEXT("data"), Data);
		return Reply;
	}

	TSharedRef<FJsonObject> MakeEventMessage(const FString& InMethod, const FString& InGateway, const FString& InTid, const FString& InBid, const TSharedPtr<FJsonObject>& InData)
	{
		TSharedRef<FJsonObject> Event = MakeShared<FJsonObject>();
		Event->SetStringField(TEXT("tid"), InTid.IsEmpty() ? NewUuid() : InTid);
		Event->SetStringField(TEXT("bid"), InBid.IsEmpty() ? NewUuid() : InBid);
		Event->SetNumberField(TEXT("timestamp"), static_cast<double>(NowTimestampMs()));
		Event->SetStringField(TEXT("gateway"), InGateway);
		Event->SetStringField(TEXT("method"), InMethod);
		if (InData.IsValid())
		{
			Event->SetObjectField(TEXT("data"), InData);
		}
		return Event;
	}

	TSharedRef<FJsonObject> MakeTelemetryHeader()
	{
		TSharedRef<FJsonObject> Header = MakeShared<FJsonObject>();
		Header->SetStringField(TEXT("tid"), NewUuid());
		Header->SetStringField(TEXT("bid"), NewUuid());
		Header->SetNumberField(TEXT("timestamp"), static_cast<double>(NowTimestampMs()));
		return Header;
	}
}
