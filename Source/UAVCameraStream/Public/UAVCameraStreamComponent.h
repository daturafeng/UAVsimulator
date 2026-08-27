// 相机载荷模拟与推流：直播会话模型（video_id、RTMP 地址、清晰度/镜头）
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Dom/JsonObject.h"
#include "UAVCameraStreamComponent.generated.h"

/** 直播会话 */
USTRUCT(BlueprintType)
struct FUAVLiveSession
{
	GENERATED_BODY()

	/** video_id（{droneSn}/{cameraIndex}/{videoType}-0） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Live")
	FString VideoId;

	/** RTMP 推流地址（rtmpBaseUrl + {droneSn}-{cameraIndex}，或指令中 url 指定） */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Live")
	FString RtmpUrl;

	/** 清晰度：0 自适应 / 1 流畅 / 2 标清 / 3 高清 / 4 超清 */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Live")
	int32 VideoQuality = 0;

	/** 镜头类型：normal / thermal / wide / zoom */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Live")
	FString VideoType = TEXT("zoom");

	/** 推流中 */
	UPROPERTY(BlueprintReadOnly, Category = "UAV|Live")
	bool bStreaming = false;
};

/** 直播状态变更事件（供桥接层转发 state/live_status；参数为变更后的 video_id） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUAVLiveStatusChangedDelegate, const FString&, VideoId);

/** 直播指令处理结果事件（Method + result 码） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUAVLiveCommandResultDelegate, const FString&, Method, int32, Result);

/**
 * 相机载荷模拟组件：维护直播会话模型，处理 live_start_push / live_stop_push /
 * live_set_quality / live_lens_change 指令。真实 RTMP 编码推流在后续变更接入。
 */
UCLASS(ClassGroup = (UAV), meta = (BlueprintSpawnableComponent))
class UAVCAMERASTREAM_API UUAVCameraStreamComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUAVCameraStreamComponent();

	// ---- 配置 ----
	/** 无人机 SN（M4TD） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Live")
	FString DroneSn = TEXT("1581F8HGXTEST001");

	/** 相机载荷索引（M4TD） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Live")
	FString CameraIndex = TEXT("52-0-0");

	/** RTMP 服务器基础地址（如 rtmp://127.0.0.1:1935/live/）；指令携带 url 时优先使用指令 url */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Live")
	FString RtmpBaseUrl = TEXT("rtmp://127.0.0.1:1935/live/");

	// ---- 事件 ----
	/** 直播状态变更事件 */
	UPROPERTY(BlueprintAssignable, Category = "UAV|Live|Event")
	FUAVLiveStatusChangedDelegate OnLiveStatusChanged;

	/** 指令处理结果事件 */
	UPROPERTY(BlueprintAssignable, Category = "UAV|Live|Event")
	FUAVLiveCommandResultDelegate OnCommandResult;

	// ---- 指令接口 ----
	/**
	 * 处理 live_* 指令（InMethod 为 method，InDataJson 为指令 data 字段 JSON 字符串）。
	 * 返回 result 码（0=成功）。
	 */
	UFUNCTION(BlueprintCallable, Category = "UAV|Live")
	int32 HandleCommand(const FString& InMethod, const FString& InDataJson);

	/** 直播会话数量 */
	UFUNCTION(BlueprintPure, Category = "UAV|Live")
	int32 GetSessionCount() const { return Sessions.Num(); }

	/** 按 video_id 查询直播会话；不存在返回 false */
	UFUNCTION(BlueprintCallable, Category = "UAV|Live")
	bool GetSession(const FString& InVideoId, FUAVLiveSession& OutSession) const;

protected:
	// ---- 指令处理 ----
	int32 HandleLiveStartPush(const TSharedPtr<FJsonObject>& InData);
	int32 HandleLiveStopPush(const TSharedPtr<FJsonObject>& InData);
	int32 HandleLiveSetQuality(const TSharedPtr<FJsonObject>& InData);
	int32 HandleLiveLensChange(const TSharedPtr<FJsonObject>& InData);

	/** 查找会话（非 const 版本） */
	FUAVLiveSession* FindSession(const FString& InVideoId);

	/** 从 video_id 解析镜头类型（{sn}/{cameraIndex}/{videoType}-0 → videoType） */
	FString ParseVideoTypeFromVideoId(const FString& InVideoId) const;

private:
	/** 直播会话列表 */
	TArray<FUAVLiveSession> Sessions;
};
