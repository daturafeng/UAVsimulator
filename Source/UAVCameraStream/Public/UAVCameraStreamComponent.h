// 相机载荷模拟与推流：直播会话模型 + FFmpeg 真实 RTMP 推流管线
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Dom/JsonObject.h"
#include "UAVCameraStreamComponent.generated.h"

class UTextureRenderTarget2D;
class USceneCaptureComponent2D;

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
 * live_set_quality / live_lens_change 指令，并通过 FFmpeg 子进程把 RenderTarget 画面
 * 编码为 H.264/FLV 推送到 RTMP 服务器（首期单会话真实推流）。
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

	/** 画面源 RenderTarget；未配置时 BeginPlay 自动创建 SceneCapture2D + RenderTarget */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Live")
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	/** FFmpeg 可执行文件路径；为空时依次探测 PATH 与常见安装目录 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Live")
	FString FfmpegPath;

	/** 严格模式：FFmpeg 不可用时 live_start_push 返回失败；默认 false（降级为占位推流） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Live")
	bool bRequireFfmpeg = false;

	/** 自动创建 RenderTarget 的默认宽度（未配置 RenderTarget 时使用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Live")
	int32 AutoRenderTargetWidth = 1280;

	/** 自动创建 RenderTarget 的默认高度（未配置 RenderTarget 时使用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Live")
	int32 AutoRenderTargetHeight = 720;

	/** FFmpeg 意外退出后的最大重连次数（0=禁用自动重连，保持"失败即停止"旧行为；默认 3） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Live", meta = (ClampMin = "0"))
	int32 MaxReconnectAttempts = 3;

	/** 重连基础间隔（秒），按指数退避递增 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Live", meta = (ClampMin = "0.1"))
	double ReconnectIntervalSeconds = 5.0;

	/** 重连退避间隔上限（秒），防止无限拉长 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UAV|Live", meta = (ClampMin = "0.1"))
	double ReconnectMaxIntervalSeconds = 30.0;

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
	// ---- 生命周期 ----
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---- 指令处理 ----
	int32 HandleLiveStartPush(const TSharedPtr<FJsonObject>& InData);
	int32 HandleLiveStopPush(const TSharedPtr<FJsonObject>& InData);
	int32 HandleLiveSetQuality(const TSharedPtr<FJsonObject>& InData);
	int32 HandleLiveLensChange(const TSharedPtr<FJsonObject>& InData);

	/** 查找会话（非 const 版本） */
	FUAVLiveSession* FindSession(const FString& InVideoId);

	/** 从 video_id 解析镜头类型（{sn}/{cameraIndex}/{videoType}-0 → videoType） */
	FString ParseVideoTypeFromVideoId(const FString& InVideoId) const;

	// ---- 推流管线 ----
	/** 启动会话推流（FFmpeg 探测/降级/子进程启动）；返回 false 表示未真实推流（占位或失败） */
	bool StartStreaming(FUAVLiveSession& InSession);

	/** 停止会话推流并回收子进程与管道 */
	void StopStreaming(FUAVLiveSession& InSession);

	/** 按会话档位写入一帧（游戏线程 Tick 节流调用） */
	void WriteSessionFrame(const FUAVLiveSession& InSession);

	/** 探测 FFmpeg 可执行文件路径（配置路径 → PATH → 常见安装目录）；不可用返回空串 */
	FString ResolveFfmpegPath() const;

	/** 确保画面源有效：未配置时自动创建 SceneCapture2D + RenderTarget */
	void EnsureRenderTarget();

	/** 取消待执行的重连 Timer 并清空重连状态 */
	void CancelReconnect();

	/** 按退避策略调度下一次重连 */
	void ScheduleReconnect();

	/** 重连 Timer 回调：按会话当前档位重新启动推流 */
	void OnReconnectTimer();

private:
	/** 直播会话列表 */
	TArray<FUAVLiveSession> Sessions;

	/** 自动创建的场景捕获组件（仅未配置 RenderTarget 时创建） */
	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> AutoCapture;

	/** 自动创建的渲染目标（仅未配置 RenderTarget 时创建） */
	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> AutoRenderTarget;

	/** 当前活动推流的 video_id（空表示无真实推流；首期单会话） */
	FString ActiveStreamVideoId;

	/** FFmpeg 子进程句柄 */
	FProcHandle FfmpegProcess;

	/** stdin 写管道（喂帧） */
	void* StdinWritePipe = nullptr;

	/** stdout 读管道（日志） */
	void* StdoutReadPipe = nullptr;

	/** 上次写帧时间（秒） */
	float LastFrameWriteTime = 0.0f;

	/** 重连目标的 video_id（重连等待期间 ActiveStreamVideoId 为空） */
	FString ReconnectVideoId;

	/** 已用重连次数 */
	int32 ReconnectAttempts = 0;

	/** 重连 Timer 句柄 */
	FTimerHandle ReconnectTimerHandle;
};
