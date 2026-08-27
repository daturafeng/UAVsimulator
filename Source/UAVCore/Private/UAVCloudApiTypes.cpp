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
	const TCHAR* kTopicDrcDownTemplate = TEXT("thing/product/{sn}/drc/down");
	const TCHAR* kTopicDrcUpTemplate = TEXT("thing/product/{sn}/drc/up");

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
	const TCHAR* kMethodFlyToPoint = TEXT("fly_to_point");
	const TCHAR* kMethodFlyToPointStop = TEXT("fly_to_point_stop");
	const TCHAR* kMethodFlyToPointUpdate = TEXT("fly_to_point_update");
	const TCHAR* kMethodLiveStartPush = TEXT("live_start_push");
	const TCHAR* kMethodLiveStopPush = TEXT("live_stop_push");
	const TCHAR* kMethodLiveSetQuality = TEXT("live_set_quality");
	const TCHAR* kMethodLiveLensChange = TEXT("live_lens_change");
	const TCHAR* kMethodPayloadAuthorityGrab = TEXT("payload_authority_grab");
	const TCHAR* kMethodCameraModeSwitch = TEXT("camera_mode_switch");
	const TCHAR* kMethodCameraPhotoTake = TEXT("camera_photo_take");
	const TCHAR* kMethodCameraPhotoStop = TEXT("camera_photo_stop");
	const TCHAR* kMethodCameraRecordingStart = TEXT("camera_recording_start");
	const TCHAR* kMethodCameraRecordingStop = TEXT("camera_recording_stop");
	const TCHAR* kMethodCameraAim = TEXT("camera_aim");
	const TCHAR* kMethodGimbalReset = TEXT("gimbal_reset");
	const TCHAR* kMethodCameraLookAt = TEXT("camera_look_at");
	const TCHAR* kMethodCameraScreenSplit = TEXT("camera_screen_split");
	const TCHAR* kMethodPhotoStorageSet = TEXT("photo_storage_set");
	const TCHAR* kMethodVideoStorageSet = TEXT("video_storage_set");
	const TCHAR* kMethodCameraExposureSet = TEXT("camera_exposure_set");
	const TCHAR* kMethodCameraExposureModeSet = TEXT("camera_exposure_mode_set");
	const TCHAR* kMethodCameraFocusModeSet = TEXT("camera_focus_mode_set");
	const TCHAR* kMethodCameraFocusValueSet = TEXT("camera_focus_value_set");
	const TCHAR* kMethodIrMeteringModeSet = TEXT("ir_metering_mode_set");
	const TCHAR* kMethodIrMeteringPointSet = TEXT("ir_metering_point_set");
	const TCHAR* kMethodIrMeteringAreaSet = TEXT("ir_metering_area_set");
	const TCHAR* kMethodCameraPointFocusAction = TEXT("camera_point_focus_action");
	const TCHAR* kMethodCameraFocalLengthSet = TEXT("camera_focal_length_set");
	const TCHAR* kMethodPoiModeEnter = TEXT("poi_mode_enter");
	const TCHAR* kMethodPoiModeExit = TEXT("poi_mode_exit");
	const TCHAR* kMethodPoiCircleSpeedSet = TEXT("poi_circle_speed_set");
	const TCHAR* kMethodDrcModeEnter = TEXT("drc_mode_enter");
	const TCHAR* kMethodDrcModeExit = TEXT("drc_mode_exit");
	const TCHAR* kMethodDroneControl = TEXT("drone_control");
	const TCHAR* kMethodHeartBeat = TEXT("heart_beat");
	const TCHAR* kMethodDroneEmergencyStop = TEXT("drone_emergency_stop");

	// ---- 远程调试/设备控制 method（对齐 dock DebugMethodEnum）----
	const TCHAR* kMethodDebugModeOpen = TEXT("debug_mode_open");
	const TCHAR* kMethodDebugModeClose = TEXT("debug_mode_close");
	const TCHAR* kMethodSupplementLightOpen = TEXT("supplement_light_open");
	const TCHAR* kMethodSupplementLightClose = TEXT("supplement_light_close");
	const TCHAR* kMethodDeviceReboot = TEXT("device_reboot");
	const TCHAR* kMethodDroneOpen = TEXT("drone_open");
	const TCHAR* kMethodDroneClose = TEXT("drone_close");
	const TCHAR* kMethodDroneFormat = TEXT("drone_format");
	const TCHAR* kMethodDeviceFormat = TEXT("device_format");
	const TCHAR* kMethodCoverOpen = TEXT("cover_open");
	const TCHAR* kMethodCoverClose = TEXT("cover_close");
	const TCHAR* kMethodPutterOpen = TEXT("putter_open");
	const TCHAR* kMethodPutterClose = TEXT("putter_close");
	const TCHAR* kMethodChargeOpen = TEXT("charge_open");
	const TCHAR* kMethodChargeClose = TEXT("charge_close");
	const TCHAR* kMethodBatteryMaintenanceSwitch = TEXT("battery_maintenance_switch");
	const TCHAR* kMethodAlarmStateSwitch = TEXT("alarm_state_switch");
	const TCHAR* kMethodBatteryStoreModeSwitch = TEXT("battery_store_mode_switch");
	const TCHAR* kMethodSdrWorkmodeSwitch = TEXT("sdr_workmode_switch");
	const TCHAR* kMethodAirConditionerModeSwitch = TEXT("air_conditioner_mode_switch");

	// ---- 默认设备标识（与 dock script/common.py 假设备口径一致）----
	const TCHAR* kDefaultDockSn = TEXT("DOCK3TEST001");
	const TCHAR* kDefaultDroneSn = TEXT("1581F8HGXTEST001");
	const TCHAR* kDefaultCameraIndex = TEXT("52-0-0");
	const TCHAR* kDefaultVideoType = TEXT("zoom");

	// ---- 事件 method ----
	const TCHAR* kEventTakeoffToPointProgress = TEXT("takeoff_to_point_progress");
	const TCHAR* kEventFlighttaskProgress = TEXT("flighttask_progress");
	const TCHAR* kEventReturnHomeInfo = TEXT("return_home_info");
	const TCHAR* kEventFlighttaskReady = TEXT("flighttask_ready");
	const TCHAR* kEventHms = TEXT("hms");
	const TCHAR* kEventFlyToPointProgress = TEXT("fly_to_point_progress");
	const TCHAR* kEventDrcStatusNotify = TEXT("drc_status_notify");

	FUAVVideoQualityParams GetVideoQualityParams(int32 InQuality)
	{
		FUAVVideoQualityParams Params;
		switch (InQuality)
		{
		case 1: Params = { 640, 360, 800, 15 }; break;	// 流畅
		case 2: Params = { 1280, 720, 2000, 15 }; break;	// 标清
		case 3: Params = { 1920, 1080, 4000, 15 }; break;	// 高清
		case 4: Params = { 1920, 1080, 6000, 15 }; break;	// 超清
		default: Params = { 1280, 720, 2000, 15 }; break;	// 自适应
		}
		return Params;
	}

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
