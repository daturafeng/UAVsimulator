// 上云 API 协议常量与报文工具
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * 大疆上云 API（Cloud API）协议常量与轻量报文工具。
 * 首期面向 M4TD 无人机 + dock3 机场，与 dock 项目联调口径一致。
 */
namespace UAV::CloudApi
{
	// ---- MQTT Topic 模板（{sn} 占位）----
	UAVCORE_API extern const TCHAR* kTopicServicesTemplate;		// thing/product/{sn}/services
	UAVCORE_API extern const TCHAR* kTopicServicesReplyTemplate;	// thing/product/{sn}/services_reply
	UAVCORE_API extern const TCHAR* kTopicEventsTemplate;			// thing/product/{sn}/events
	UAVCORE_API extern const TCHAR* kTopicOsdTemplate;				// thing/product/{sn}/osd
	UAVCORE_API extern const TCHAR* kTopicStateTemplate;			// thing/product/{sn}/state
	UAVCORE_API extern const TCHAR* kTopicDrcDownTemplate;			// thing/product/{sn}/drc/down（云端 → 设备 DRC 指令）
	UAVCORE_API extern const TCHAR* kTopicDrcUpTemplate;			// thing/product/{sn}/drc/up（设备 → 云端 DRC 回执）

	// ---- 服务指令 method ----
	UAVCORE_API extern const TCHAR* kMethodFlightAuthorityGrab;		// flight_authority_grab
	UAVCORE_API extern const TCHAR* kMethodTakeoffToPoint;			// takeoff_to_point
	UAVCORE_API extern const TCHAR* kMethodFlighttaskCreate;		// flighttask_create
	UAVCORE_API extern const TCHAR* kMethodFlighttaskPrepare;		// flighttask_prepare
	UAVCORE_API extern const TCHAR* kMethodFlighttaskExecute;		// flighttask_execute
	UAVCORE_API extern const TCHAR* kMethodFlighttaskUndo;			// flighttask_undo
	UAVCORE_API extern const TCHAR* kMethodFlighttaskPause;			// flighttask_pause
	UAVCORE_API extern const TCHAR* kMethodFlighttaskRecovery;		// flighttask_recovery
	UAVCORE_API extern const TCHAR* kMethodReturnHome;				// return_home
	UAVCORE_API extern const TCHAR* kMethodReturnHomeCancel;		// return_home_cancel
	UAVCORE_API extern const TCHAR* kMethodFlyToPoint;				// fly_to_point
	UAVCORE_API extern const TCHAR* kMethodFlyToPointStop;			// fly_to_point_stop
	UAVCORE_API extern const TCHAR* kMethodFlyToPointUpdate;		// fly_to_point_update
	UAVCORE_API extern const TCHAR* kMethodLiveStartPush;			// live_start_push
	UAVCORE_API extern const TCHAR* kMethodLiveStopPush;			// live_stop_push
	UAVCORE_API extern const TCHAR* kMethodLiveSetQuality;			// live_set_quality
	UAVCORE_API extern const TCHAR* kMethodLiveLensChange;			// live_lens_change
	UAVCORE_API extern const TCHAR* kMethodPayloadAuthorityGrab;	// payload_authority_grab
	UAVCORE_API extern const TCHAR* kMethodCameraModeSwitch;		// camera_mode_switch
	UAVCORE_API extern const TCHAR* kMethodCameraPhotoTake;			// camera_photo_take
	UAVCORE_API extern const TCHAR* kMethodCameraPhotoStop;			// camera_photo_stop
	UAVCORE_API extern const TCHAR* kMethodCameraRecordingStart;	// camera_recording_start
	UAVCORE_API extern const TCHAR* kMethodCameraRecordingStop;		// camera_recording_stop
	UAVCORE_API extern const TCHAR* kMethodCameraAim;				// camera_aim
	UAVCORE_API extern const TCHAR* kMethodGimbalReset;				// gimbal_reset
	UAVCORE_API extern const TCHAR* kMethodCameraLookAt;			// camera_look_at
	UAVCORE_API extern const TCHAR* kMethodCameraScreenSplit;		// camera_screen_split
	UAVCORE_API extern const TCHAR* kMethodPhotoStorageSet;			// photo_storage_set
	UAVCORE_API extern const TCHAR* kMethodVideoStorageSet;			// video_storage_set
	UAVCORE_API extern const TCHAR* kMethodCameraExposureSet;		// camera_exposure_set
	UAVCORE_API extern const TCHAR* kMethodCameraExposureModeSet;	// camera_exposure_mode_set
	UAVCORE_API extern const TCHAR* kMethodCameraFocusModeSet;		// camera_focus_mode_set
	UAVCORE_API extern const TCHAR* kMethodCameraFocusValueSet;		// camera_focus_value_set
	UAVCORE_API extern const TCHAR* kMethodIrMeteringModeSet;		// ir_metering_mode_set
	UAVCORE_API extern const TCHAR* kMethodIrMeteringPointSet;		// ir_metering_point_set
	UAVCORE_API extern const TCHAR* kMethodIrMeteringAreaSet;		// ir_metering_area_set
	UAVCORE_API extern const TCHAR* kMethodCameraPointFocusAction;	// camera_point_focus_action
	UAVCORE_API extern const TCHAR* kMethodCameraFocalLengthSet;	// camera_focal_length_set
	UAVCORE_API extern const TCHAR* kMethodPoiModeEnter;			// poi_mode_enter
	UAVCORE_API extern const TCHAR* kMethodPoiModeExit;				// poi_mode_exit
	UAVCORE_API extern const TCHAR* kMethodPoiCircleSpeedSet;		// poi_circle_speed_set
	UAVCORE_API extern const TCHAR* kMethodDrcModeEnter;			// drc_mode_enter（进入 DRC 直控）
	UAVCORE_API extern const TCHAR* kMethodDrcModeExit;				// drc_mode_exit（退出 DRC 直控）
	UAVCORE_API extern const TCHAR* kMethodDroneControl;			// drone_control（虚拟摇杆指令）
	UAVCORE_API extern const TCHAR* kMethodHeartBeat;				// heart_beat（DRC 心跳）
	UAVCORE_API extern const TCHAR* kMethodDroneEmergencyStop;		// drone_emergency_stop（DRC 急停）

	// ---- 默认设备标识（首期联调口径，与 dock script 一致）----
	UAVCORE_API extern const TCHAR* kDefaultDockSn;			// DOCK3TEST001
	UAVCORE_API extern const TCHAR* kDefaultDroneSn;		// 1581F8HGXTEST001
	UAVCORE_API extern const TCHAR* kDefaultCameraIndex;	// 52-0-0（M4TD 相机载荷索引）
	UAVCORE_API extern const TCHAR* kDefaultVideoType;		// zoom

	// ---- 事件 method ----
	UAVCORE_API extern const TCHAR* kEventTakeoffToPointProgress;	// takeoff_to_point_progress
	UAVCORE_API extern const TCHAR* kEventFlighttaskProgress;		// flighttask_progress
	UAVCORE_API extern const TCHAR* kEventReturnHomeInfo;			// return_home_info（对齐 dock EventsMethodEnum）
	UAVCORE_API extern const TCHAR* kEventFlighttaskReady;			// flighttask_ready
	UAVCORE_API extern const TCHAR* kEventHms;						// hms
	UAVCORE_API extern const TCHAR* kEventFlyToPointProgress;		// fly_to_point_progress
	UAVCORE_API extern const TCHAR* kEventDrcStatusNotify;			// drc_status_notify（DRC 会话状态）

	// ---- 视频清晰度档位（与上云 API video_quality 语义一致）----
	/** 推流参数（分辨率/码率/帧率） */
	struct UAVCORE_API FUAVVideoQualityParams
	{
		/** 宽（像素） */
		int32 Width = 0;
		/** 高（像素） */
		int32 Height = 0;
		/** 码率（kbps） */
		int32 BitrateKbps = 0;
		/** 帧率（fps） */
		int32 Fps = 0;
	};

	/** 按 video_quality（0=自适应/1=流畅/2=标清/3=高清/4=超清）返回推流参数；非法档位回退自适应 */
	UAVCORE_API FUAVVideoQualityParams GetVideoQualityParams(int32 InQuality);

	// ---- 工具 ----
	/** 将 {sn} 占位替换为具体设备 SN */
	UAVCORE_API FString MakeTopic(const FString& InTemplate, const FString& InSn);

	/** 组装 video_id：{droneSn}/{cameraIndex}/{videoType}-0 */
	UAVCORE_API FString MakeVideoId(const FString& InDroneSn, const FString& InCameraIndex, const FString& InVideoType);

	/** 组装 RTMP 推流地址：{rtmpBaseUrl}{droneSn}-{cameraIndex} */
	UAVCORE_API FString MakeRtmpPushUrl(const FString& InRtmpBaseUrl, const FString& InDroneSn, const FString& InCameraIndex);

	/** 生成 UUID（小写 + 连字符，与 tid/bid 格式一致） */
	UAVCORE_API FString NewUuid();

	/** 当前 UTC 毫秒时间戳 */
	UAVCORE_API int64 NowTimestampMs();

	/**
	 * 构造上云 API 报文头：{tid, bid, timestamp, method, data}。
	 * InBid 为空时自动生成；gateway 由桥接层（MQTT 模块）按设备 SN 补充。
	 */
	UAVCORE_API TSharedRef<FJsonObject> MakeMessageHeader(const FString& InMethod, const FString& InBid = FString(), const TSharedPtr<FJsonObject>& InData = nullptr);

	/**
	 * 构造 services 服务回复报文：{tid, bid, method, timestamp, data:{result, output?}}。
	 * 用于回复 dock 下发的 services 指令（与 dock 构造服务回复口径一致）。
	 */
	UAVCORE_API TSharedRef<FJsonObject> MakeServicesReply(const FString& InMethod, const FString& InTid, const FString& InBid, int32 InResult, const TSharedPtr<FJsonObject>& InOutput = nullptr);

	/**
	 * 构造事件报文：{tid, bid, timestamp, gateway, method, data}。
	 * 用于 takeoff_to_point_progress / flighttask_progress / live_status 等上行事件。
	 */
	UAVCORE_API TSharedRef<FJsonObject> MakeEventMessage(const FString& InMethod, const FString& InGateway, const FString& InTid, const FString& InBid, const TSharedPtr<FJsonObject>& InData);

	/**
	 * 构造遥测/状态报文头：{tid, bid, timestamp}（无 method，用于 OSD/state/status）。
	 */
	UAVCORE_API TSharedRef<FJsonObject> MakeTelemetryHeader();
}
