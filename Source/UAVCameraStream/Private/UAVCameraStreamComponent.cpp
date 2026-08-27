// 相机载荷模拟与推流：会话模型 + FFmpeg 真实 RTMP 推流管线实现
#include "UAVCameraStreamComponent.h"
#include "UAVCloudApiTypes.h"
#include "UAVFfmpegCommand.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

namespace
{
	/** 直播指令结果码（0=成功） */
	constexpr int32 kResultSuccess = 0;
	constexpr int32 kResultInvalidUrlType = 1;
	constexpr int32 kResultSessionNotFound = 2;
	constexpr int32 kResultInvalidParams = 3;
	constexpr int32 kResultUnknownMethod = 4;
	constexpr int32 kResultFfmpegUnavailable = 5;
	/** 载荷组件未注入无人机模拟（内部错误） */
	constexpr int32 kResultInternalError = 6;
	/** 未抢占载荷权（对齐飞控 NoAuthority result=2） */
	constexpr int32 kResultNoAuthority = 2;

	bool TryParseJsonObject(const FString& InJson, TSharedPtr<FJsonObject>& OutObject)
	{
		if (InJson.IsEmpty())
		{
			return false;
		}
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InJson);
		return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
	}

	double ReadNumber(const TSharedPtr<FJsonObject>& InData, const TCHAR* InKey, double InDefault = 0.0)
	{
		double Value = InDefault;
		if (InData.IsValid())
		{
			InData->TryGetNumberField(InKey, Value);
		}
		return Value;
	}

	FString ReadString(const TSharedPtr<FJsonObject>& InData, const TCHAR* InKey, const TCHAR* InDefault = TEXT(""))
	{
		FString Value = InDefault;
		if (InData.IsValid())
		{
			InData->TryGetStringField(InKey, Value);
		}
		return Value;
	}
}

UUAVCameraStreamComponent::UUAVCameraStreamComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UUAVCameraStreamComponent::SetDroneSim(UUAVDroneSimComponent* InDroneSim)
{
	DroneSim = InDroneSim;
}

void UUAVCameraStreamComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureRenderTarget();
}

void UUAVCameraStreamComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 兜底清理：停止所有推流并回收子进程与管道
	CancelReconnect();
	for (FUAVLiveSession& Session : Sessions)
	{
		Session.bStreaming = false;
	}
	if (FfmpegProcess.IsValid())
	{
		FPlatformProcess::TerminateProc(FfmpegProcess, true);
		FPlatformProcess::CloseProc(FfmpegProcess);
		FfmpegProcess.Reset();
	}
	if (StdinWritePipe)
	{
		FPlatformProcess::ClosePipe(StdoutReadPipe, StdinWritePipe);
		StdinWritePipe = nullptr;
		StdoutReadPipe = nullptr;
	}
	ActiveStreamVideoId.Reset();
	Super::EndPlay(EndPlayReason);
}

void UUAVCameraStreamComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ActiveStreamVideoId.IsEmpty() || !FfmpegProcess.IsValid())
	{
		return;
	}

	// 检测 FFmpeg 非预期退出（如 RTMP 服务器不可达），停止会话推流并回收
	int32 ExitCode = 0;
	if (FPlatformProcess::GetProcReturnCode(FfmpegProcess, &ExitCode))
	{
		const FString ExitedVideoId = ActiveStreamVideoId;
		FPlatformProcess::CloseProc(FfmpegProcess);
		FfmpegProcess.Reset();
		if (StdinWritePipe)
		{
			FPlatformProcess::ClosePipe(StdoutReadPipe, StdinWritePipe);
			StdinWritePipe = nullptr;
			StdoutReadPipe = nullptr;
		}
		UE_LOG(LogTemp, Warning, TEXT("[UAVCameraStream] FFmpeg 意外退出（码 %d）：%s"), ExitCode, *ExitedVideoId);

		// 按策略自动重连：记录重连目标并调度，重连期间会话保持推流状态
		if (UAV::Ffmpeg::ShouldRetryReconnect(ReconnectAttempts, MaxReconnectAttempts))
		{
			ReconnectVideoId = ExitedVideoId;
			ActiveStreamVideoId.Reset();
			ScheduleReconnect();
		}
		else
		{
			if (FUAVLiveSession* Session = FindSession(ExitedVideoId))
			{
				Session->bStreaming = false;
				OnLiveStatusChanged.Broadcast(Session->VideoId);
			}
			ActiveStreamVideoId.Reset();
		}
		return;
	}

	// 按会话清晰度档位帧率节流写帧
	const FUAVLiveSession* Session = FindSession(ActiveStreamVideoId);
	if (!Session)
	{
		return;
	}
	const UAV::CloudApi::FUAVVideoQualityParams Params = UAV::CloudApi::GetVideoQualityParams(Session->VideoQuality);
	const float FrameInterval = Params.Fps > 0 ? 1.0f / static_cast<float>(Params.Fps) : 1.0f / 15.0f;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (Now - LastFrameWriteTime >= FrameInterval)
	{
		LastFrameWriteTime = Now;
		WriteSessionFrame(*Session);
	}
}

int32 UUAVCameraStreamComponent::HandleCommand(const FString& InMethod, const FString& InDataJson)
{
	using namespace UAV::CloudApi;

	TSharedPtr<FJsonObject> Data;
	TryParseJsonObject(InDataJson, Data);

	const FString Method = InMethod.TrimStartAndEnd();
	int32 Result = kResultUnknownMethod;
	if (Method == kMethodLiveStartPush)
	{
		Result = HandleLiveStartPush(Data);
	}
	else if (Method == kMethodLiveStopPush)
	{
		Result = HandleLiveStopPush(Data);
	}
	else if (Method == kMethodLiveSetQuality)
	{
		Result = HandleLiveSetQuality(Data);
	}
	else if (Method == kMethodLiveLensChange)
	{
		Result = HandleLiveLensChange(Data);
	}
	else if (Method == kMethodPayloadAuthorityGrab
		|| Method == kMethodCameraModeSwitch
		|| Method == kMethodCameraPhotoTake
		|| Method == kMethodCameraPhotoStop
		|| Method == kMethodCameraRecordingStart
		|| Method == kMethodCameraRecordingStop
		|| Method == kMethodCameraAim
		|| Method == kMethodGimbalReset)
	{
		if (!DroneSim)
		{
			Result = kResultInternalError;
		}
		else if (Method == kMethodPayloadAuthorityGrab)
		{
			Result = HandlePayloadAuthorityGrab(Data);
		}
		else if (!DroneSim->HasPayloadAuthority())
		{
			// 载荷权校验：未抢占时拒绝拍照/录像/云台指令
			Result = kResultNoAuthority;
			UE_LOG(LogTemp, Warning, TEXT("[UAVCameraStream] 载荷指令 %s 被拒绝：未抢占载荷权"), *Method);
		}
		else if (Method == kMethodCameraModeSwitch)
		{
			Result = HandleCameraModeSwitch(Data);
		}
		else if (Method == kMethodCameraPhotoTake)
		{
			Result = HandleCameraPhotoTake(Data);
		}
		else if (Method == kMethodCameraPhotoStop)
		{
			Result = HandleCameraPhotoStop(Data);
		}
		else if (Method == kMethodCameraRecordingStart)
		{
			Result = HandleCameraRecordingStart(Data);
		}
		else if (Method == kMethodCameraRecordingStop)
		{
			Result = HandleCameraRecordingStop(Data);
		}
		else if (Method == kMethodCameraAim)
		{
			Result = HandleCameraAim(Data);
		}
		else if (Method == kMethodGimbalReset)
		{
			Result = HandleGimbalReset(Data);
		}
	}

	OnCommandResult.Broadcast(Method, Result);
	return Result;
}

bool UUAVCameraStreamComponent::GetSession(const FString& InVideoId, FUAVLiveSession& OutSession) const
{
	for (const FUAVLiveSession& Session : Sessions)
	{
		if (Session.VideoId == InVideoId)
		{
			OutSession = Session;
			return true;
		}
	}
	return false;
}

FUAVLiveSession* UUAVCameraStreamComponent::FindSession(const FString& InVideoId)
{
	for (FUAVLiveSession& Session : Sessions)
	{
		if (Session.VideoId == InVideoId)
		{
			return &Session;
		}
	}
	return nullptr;
}

FString UUAVCameraStreamComponent::ParseVideoTypeFromVideoId(const FString& InVideoId) const
{
	int32 LastSlash = INDEX_NONE;
	InVideoId.FindLastChar(TEXT('/'), LastSlash);
	if (LastSlash == INDEX_NONE || LastSlash >= InVideoId.Len() - 1)
	{
		return UAV::CloudApi::kDefaultVideoType;
	}
	FString VideoType = InVideoId.RightChop(LastSlash + 1);
	VideoType.RemoveFromEnd(TEXT("-0"));
	return VideoType.IsEmpty() ? UAV::CloudApi::kDefaultVideoType : VideoType;
}

int32 UUAVCameraStreamComponent::HandleLiveStartPush(const TSharedPtr<FJsonObject>& InData)
{
	using namespace UAV::CloudApi;

	const int32 UrlType = static_cast<int32>(ReadNumber(InData, TEXT("url_type")));
	if (UrlType != 1)
	{
		// 仅支持 url_type=1（RTMP 推流）
		UE_LOG(LogTemp, Warning, TEXT("[UAVCameraStream] live_start_push 仅支持 url_type=1，收到 %d"), UrlType);
		return kResultInvalidUrlType;
	}

	const FString Url = ReadString(InData, TEXT("url"));
	const FString VideoType = ParseVideoTypeFromVideoId(ReadString(InData, TEXT("video_id")));
	FString VideoId = ReadString(InData, TEXT("video_id"));
	if (VideoId.IsEmpty())
	{
		VideoId = MakeVideoId(DroneSn, CameraIndex, VideoType);
	}
	const int32 VideoQuality = FMath::Clamp(static_cast<int32>(ReadNumber(InData, TEXT("video_quality"))), 0, 4);

	// 同一 video_id 重复开启视为已存在：保持会话，确保真实推流在跑
	if (FUAVLiveSession* Existing = FindSession(VideoId))
	{
		Existing->bStreaming = true;
		if (ActiveStreamVideoId != VideoId)
		{
			StartStreaming(*Existing);
		}
		UE_LOG(LogTemp, Log, TEXT("[UAVCameraStream] 推流已存在，保持开启：%s"), *VideoId);
		OnLiveStatusChanged.Broadcast(VideoId);
		return kResultSuccess;
	}

	FUAVLiveSession Session;
	Session.VideoId = VideoId;
	Session.RtmpUrl = Url.IsEmpty() ? MakeRtmpPushUrl(RtmpBaseUrl, DroneSn, CameraIndex) : Url;
	Session.VideoQuality = VideoQuality;
	Session.VideoType = VideoType;
	Session.bStreaming = true;
	Sessions.Add(Session);

	// 启动真实推流；严格模式下失败则回滚会话（返回非 0）
	FUAVLiveSession& Added = Sessions.Last();
	if (!StartStreaming(Added) && bRequireFfmpeg)
	{
		Sessions.Pop();
		UE_LOG(LogTemp, Warning, TEXT("[UAVCameraStream] 严格模式推流启动失败，回滚会话：%s"), *VideoId);
		return kResultFfmpegUnavailable;
	}

	UE_LOG(LogTemp, Log, TEXT("[UAVCameraStream] 开始推流：video_id=%s rtmp=%s 清晰度=%d"), *VideoId, *Session.RtmpUrl, VideoQuality);
	OnLiveStatusChanged.Broadcast(VideoId);
	return kResultSuccess;
}

int32 UUAVCameraStreamComponent::HandleLiveStopPush(const TSharedPtr<FJsonObject>& InData)
{
	const FString VideoId = ReadString(InData, TEXT("video_id"));
	FUAVLiveSession* Session = FindSession(VideoId);
	if (!Session)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UAVCameraStream] live_stop_push 未找到会话：%s"), *VideoId);
		return kResultSessionNotFound;
	}
	// 重连等待期间收到停止指令：取消重连再停止，防止旧回调复活推流
	CancelReconnect();
	StopStreaming(*Session);
	UE_LOG(LogTemp, Log, TEXT("[UAVCameraStream] 停止推流：%s"), *VideoId);
	OnLiveStatusChanged.Broadcast(VideoId);
	return kResultSuccess;
}

int32 UUAVCameraStreamComponent::HandleLiveSetQuality(const TSharedPtr<FJsonObject>& InData)
{
	const FString VideoId = ReadString(InData, TEXT("video_id"));
	const int32 VideoQuality = static_cast<int32>(ReadNumber(InData, TEXT("video_quality")));
	if (VideoQuality < 0 || VideoQuality > 4)
	{
		return kResultInvalidParams;
	}
	FUAVLiveSession* Session = FindSession(VideoId);
	if (!Session)
	{
		return kResultSessionNotFound;
	}
	Session->VideoQuality = VideoQuality;

	// 运行时切换清晰度：重启 FFmpeg 子进程应用新档位参数
	if (Session->bStreaming && ActiveStreamVideoId == VideoId)
	{
		// 若处于重连等待，先取消旧调度，再按新档位重启
		CancelReconnect();
		StopStreaming(*Session);
		Session->bStreaming = true;
		StartStreaming(*Session);
	}
	else if (Session->bStreaming && ReconnectVideoId == VideoId)
	{
		// 重连等待中切换清晰度：取消重连并按新档位立即重启
		CancelReconnect();
		Session->bStreaming = true;
		StartStreaming(*Session);
	}
	UE_LOG(LogTemp, Log, TEXT("[UAVCameraStream] 切换清晰度：%s -> %d"), *VideoId, VideoQuality);
	OnLiveStatusChanged.Broadcast(VideoId);
	return kResultSuccess;
}

int32 UUAVCameraStreamComponent::HandleLiveLensChange(const TSharedPtr<FJsonObject>& InData)
{
	using namespace UAV::CloudApi;

	const FString OldVideoId = ReadString(InData, TEXT("video_id"));
	FUAVLiveSession* Session = FindSession(OldVideoId);
	if (!Session)
	{
		return kResultSessionNotFound;
	}

	const FString VideoType = ReadString(InData, TEXT("video_type"), kDefaultVideoType);
	if (VideoType != TEXT("normal") && VideoType != TEXT("thermal") && VideoType != TEXT("wide") && VideoType != TEXT("zoom"))
	{
		return kResultInvalidParams;
	}

	Session->VideoType = VideoType;
	// 镜头类型变化后 video_id 同步更新（{sn}/{cameraIndex}/{videoType}-0）
	Session->VideoId = MakeVideoId(DroneSn, CameraIndex, VideoType);
	if (ActiveStreamVideoId == OldVideoId)
	{
		ActiveStreamVideoId = Session->VideoId;
	}
	UE_LOG(LogTemp, Log, TEXT("[UAVCameraStream] 镜头切换：%s -> %s"), *OldVideoId, *Session->VideoId);
	OnLiveStatusChanged.Broadcast(Session->VideoId);
	return kResultSuccess;
}

// ---- 载荷指令处理 ----

int32 UUAVCameraStreamComponent::HandlePayloadAuthorityGrab(const TSharedPtr<FJsonObject>& InData)
{
	DroneSim->SetPayloadAuthority(true);
	UE_LOG(LogTemp, Log, TEXT("[UAVCameraStream] 载荷权已抢占（控制源 A）"));
	return kResultSuccess;
}

int32 UUAVCameraStreamComponent::HandleCameraModeSwitch(const TSharedPtr<FJsonObject>& InData)
{
	const int32 Mode = static_cast<int32>(ReadNumber(InData, TEXT("camera_mode")));
	DroneSim->SetCameraMode(Mode);
	UE_LOG(LogTemp, Log, TEXT("[UAVCameraStream] 相机模式切换：%d"), DroneSim->GetCameraMode());
	return kResultSuccess;
}

int32 UUAVCameraStreamComponent::HandleCameraPhotoTake(const TSharedPtr<FJsonObject>& InData)
{
	DroneSim->TakePhoto();
	UE_LOG(LogTemp, Log, TEXT("[UAVCameraStream] 拍照：剩余=%d 累计=%d"),
		DroneSim->GetRemainingPhotoNum(), DroneSim->GetTakenPhotoCount());
	return kResultSuccess;
}

int32 UUAVCameraStreamComponent::HandleCameraPhotoStop(const TSharedPtr<FJsonObject>& InData)
{
	DroneSim->SetPhotoTaking(false);
	UE_LOG(LogTemp, Log, TEXT("[UAVCameraStream] 拍照结束"));
	return kResultSuccess;
}

int32 UUAVCameraStreamComponent::HandleCameraRecordingStart(const TSharedPtr<FJsonObject>& InData)
{
	DroneSim->StartRecording();
	UE_LOG(LogTemp, Log, TEXT("[UAVCameraStream] 开始录像（指令覆盖）"));
	return kResultSuccess;
}

int32 UUAVCameraStreamComponent::HandleCameraRecordingStop(const TSharedPtr<FJsonObject>& InData)
{
	DroneSim->StopRecording();
	UE_LOG(LogTemp, Log, TEXT("[UAVCameraStream] 停止录像（指令覆盖）"));
	return kResultSuccess;
}

int32 UUAVCameraStreamComponent::HandleCameraAim(const TSharedPtr<FJsonObject>& InData)
{
	const double Pitch = ReadNumber(InData, TEXT("gimbal_pitch"));
	const double Yaw = ReadNumber(InData, TEXT("gimbal_yaw"));
	DroneSim->SetGimbalTarget(Pitch, Yaw);
	UE_LOG(LogTemp, Log, TEXT("[UAVCameraStream] 云台瞄准：pitch=%.1f yaw=%.1f"), Pitch, Yaw);
	return kResultSuccess;
}

int32 UUAVCameraStreamComponent::HandleGimbalReset(const TSharedPtr<FJsonObject>& InData)
{
	DroneSim->ResetGimbalTarget();
	UE_LOG(LogTemp, Log, TEXT("[UAVCameraStream] 云台复位"));
	return kResultSuccess;
}

bool UUAVCameraStreamComponent::StartStreaming(FUAVLiveSession& InSession)
{
	// 首期单会话真实推流：切换会话前先停掉旧推流
	if (!ActiveStreamVideoId.IsEmpty() && ActiveStreamVideoId != InSession.VideoId)
	{
		if (FUAVLiveSession* Prev = FindSession(ActiveStreamVideoId))
		{
			StopStreaming(*Prev);
		}
	}

	// 画面源有效性校验（未配置时自动创建）
	EnsureRenderTarget();
	if (!RenderTarget || RenderTarget->SizeX <= 0 || RenderTarget->SizeY <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UAVCameraStream] 无有效 RenderTarget，进入占位推流：%s"), *InSession.VideoId);
		ActiveStreamVideoId = InSession.VideoId;
		return false;
	}

	const FString Ffmpeg = ResolveFfmpegPath();
	if (Ffmpeg.IsEmpty())
	{
		if (bRequireFfmpeg)
		{
			// 严格模式：推流启动失败（调用方负责回滚会话）
			InSession.bStreaming = false;
			UE_LOG(LogTemp, Warning, TEXT("[UAVCameraStream] 严格模式下未找到 FFmpeg，推流启动失败：%s"), *InSession.VideoId);
			return false;
		}
		UE_LOG(LogTemp, Warning, TEXT("[UAVCameraStream] 未找到 FFmpeg，进入占位推流（可配置 FfmpegPath）：%s"), *InSession.VideoId);
		ActiveStreamVideoId = InSession.VideoId;
		return false;
	}

	// 构造推流命令并启动 FFmpeg 子进程（stdin 管道喂帧）
	const UAV::CloudApi::FUAVVideoQualityParams Params = UAV::CloudApi::GetVideoQualityParams(InSession.VideoQuality);
	const FString Command = UAV::Ffmpeg::MakePushCommand(Ffmpeg, Params, InSession.RtmpUrl, RenderTarget->SizeX, RenderTarget->SizeY);

	void* ParentRead = nullptr;
	void* ParentWrite = nullptr;
	FPlatformProcess::CreatePipe(ParentRead, ParentWrite);
	uint32 ProcessId = 0;
	// 参数顺序：PipeWriteChild=父读管道（子 stdout），PipeReadChild=父写管道（子 stdin）
	FProcHandle Proc = FPlatformProcess::CreateProc(*Ffmpeg, *Command, false, true, true, &ProcessId, 0, nullptr, ParentRead, ParentWrite);
	if (!Proc.IsValid())
	{
		FPlatformProcess::ClosePipe(ParentRead, ParentWrite);
		InSession.bStreaming = false;
		if (bRequireFfmpeg)
		{
			UE_LOG(LogTemp, Warning, TEXT("[UAVCameraStream] FFmpeg 启动失败，推流启动失败：%s"), *InSession.VideoId);
			return false;
		}
		UE_LOG(LogTemp, Warning, TEXT("[UAVCameraStream] FFmpeg 启动失败，进入占位推流：%s"), *InSession.VideoId);
		ActiveStreamVideoId = InSession.VideoId;
		return false;
	}

	// 替换旧句柄（正常路径下旧推流已停止）
	FfmpegProcess = Proc;
	StdinWritePipe = ParentWrite;
	StdoutReadPipe = ParentRead;
	ActiveStreamVideoId = InSession.VideoId;
	LastFrameWriteTime = 0.0f;
	UE_LOG(LogTemp, Log, TEXT("[UAVCameraStream] FFmpeg 推流已启动：%s pid=%u 命令=%s"), *InSession.VideoId, ProcessId, *Command);
	return true;
}

void UUAVCameraStreamComponent::StopStreaming(FUAVLiveSession& InSession)
{
	InSession.bStreaming = false;
	if (ActiveStreamVideoId != InSession.VideoId)
	{
		return;
	}

	if (FfmpegProcess.IsValid())
	{
		// 关闭 stdin 触发 EOF 后强制终止并回收
		if (StdinWritePipe)
		{
			FPlatformProcess::ClosePipe(StdoutReadPipe, StdinWritePipe);
			StdinWritePipe = nullptr;
			StdoutReadPipe = nullptr;
		}
		int32 ExitCode = 0;
		if (!FPlatformProcess::GetProcReturnCode(FfmpegProcess, &ExitCode))
		{
			FPlatformProcess::TerminateProc(FfmpegProcess, true);
		}
		FPlatformProcess::CloseProc(FfmpegProcess);
		FfmpegProcess.Reset();
		UE_LOG(LogTemp, Log, TEXT("[UAVCameraStream] FFmpeg 推流已停止：%s（退出码 %d）"), *InSession.VideoId, ExitCode);
	}
	ActiveStreamVideoId.Reset();
	LastFrameWriteTime = 0.0f;
}

void UUAVCameraStreamComponent::WriteSessionFrame(const FUAVLiveSession& InSession)
{
	if (!FfmpegProcess.IsValid() || !StdinWritePipe || !RenderTarget)
	{
		return;
	}
	if (RenderTarget->SizeX <= 0 || RenderTarget->SizeY <= 0)
	{
		return;
	}

	// 同步读回 BGRA 像素（FColor 内存布局为 B/G/R/A），写入 FFmpeg stdin
	FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!RTResource)
	{
		return;
	}
	TArray<FColor> Pixels;
	RTResource->ReadPixels(Pixels);
	if (Pixels.Num() == 0)
	{
		return;
	}
	FPlatformProcess::WritePipe(StdinWritePipe, reinterpret_cast<const uint8*>(Pixels.GetData()), Pixels.Num() * static_cast<int32>(sizeof(FColor)));
}

FString UUAVCameraStreamComponent::ResolveFfmpegPath() const
{
	// 1. 显式配置路径
	if (!FfmpegPath.IsEmpty())
	{
		if (FPaths::FileExists(FfmpegPath))
		{
			return FfmpegPath;
		}
		UE_LOG(LogTemp, Warning, TEXT("[UAVCameraStream] 配置的 FFmpeg 路径不存在：%s"), *FfmpegPath);
		return FString();
	}

	// 2. PATH 环境变量
	const FString PathEnv = FPlatformMisc::GetEnvironmentVariable(TEXT("PATH"));
	TArray<FString> PathDirs;
	PathEnv.ParseIntoArray(PathDirs, TEXT(";"), true);
	for (const FString& Dir : PathDirs)
	{
		if (Dir.IsEmpty())
		{
			continue;
		}
		const FString Candidate = FPaths::Combine(Dir, TEXT("ffmpeg.exe"));
		if (FPaths::FileExists(Candidate))
		{
			return Candidate;
		}
	}

	// 3. 常见安装目录
	static const TCHAR* kCommonFfmpegPaths[] = {
		TEXT("C:/ffmpeg/bin/ffmpeg.exe"),
		TEXT("C:/Program Files/ffmpeg/bin/ffmpeg.exe"),
		TEXT("D:/ffmpeg/bin/ffmpeg.exe"),
		TEXT("D:/soft/ffmpeg/bin/ffmpeg.exe"),
	};
	for (const TCHAR* Candidate : kCommonFfmpegPaths)
	{
		if (FPaths::FileExists(Candidate))
		{
			return Candidate;
		}
	}
	return FString();
}

void UUAVCameraStreamComponent::EnsureRenderTarget()
{
	if (RenderTarget)
	{
		return;
	}
	AActor* Owner = GetOwner();
	if (!Owner || !GetWorld())
	{
		return;
	}

	// 自动创建渲染目标与场景捕获组件（挂到无人机 Actor）
	UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(Owner);
	RT->InitAutoFormat(AutoRenderTargetWidth, AutoRenderTargetHeight);
	RT->UpdateResource();
	AutoRenderTarget = RT;
	RenderTarget = RT;

	USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(Owner, USceneCaptureComponent2D::StaticClass(), TEXT("UAVAutoSceneCapture"));
	if (USceneComponent* Root = Owner->GetRootComponent())
	{
		Capture->SetupAttachment(Root);
	}
	Capture->bCaptureEveryFrame = true;
	Capture->bCaptureOnMovement = true;
	Capture->CaptureSource = SCS_FinalColorLDR;
	Capture->TextureTarget = RT;
	Capture->RegisterComponent();
	AutoCapture = Capture;
	UE_LOG(LogTemp, Log, TEXT("[UAVCameraStream] 自动创建 SceneCapture2D + RenderTarget：%dx%d"), AutoRenderTargetWidth, AutoRenderTargetHeight);
}

void UUAVCameraStreamComponent::CancelReconnect()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReconnectTimerHandle);
	}
	ReconnectTimerHandle.Invalidate();
	ReconnectVideoId.Reset();
	ReconnectAttempts = 0;
}

void UUAVCameraStreamComponent::ScheduleReconnect()
{
	if (ReconnectVideoId.IsEmpty())
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const double Delay = UAV::Ffmpeg::NextReconnectDelaySeconds(ReconnectAttempts, ReconnectIntervalSeconds, ReconnectMaxIntervalSeconds);
	World->GetTimerManager().SetTimer(
		ReconnectTimerHandle,
		FTimerDelegate::CreateUObject(this, &UUAVCameraStreamComponent::OnReconnectTimer),
		Delay,
		false);
	UE_LOG(LogTemp, Log, TEXT("[UAVCameraStream] 计划重连（第 %d 次，%.1f 秒后）：%s"), ReconnectAttempts + 1, Delay, *ReconnectVideoId);
}

void UUAVCameraStreamComponent::OnReconnectTimer()
{
	ReconnectTimerHandle.Invalidate();
	if (ReconnectVideoId.IsEmpty())
	{
		return;
	}

	FUAVLiveSession* Session = FindSession(ReconnectVideoId);
	if (!Session || !Session->bStreaming)
	{
		// 会话已不存在或已停止（例如重连等待期间被停止），放弃重连
		CancelReconnect();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[UAVCameraStream] 尝试重连推流：%s"), *Session->VideoId);
	if (StartStreaming(*Session))
	{
		// 重连成功：清空重连状态（StartStreaming 已恢复 ActiveStreamVideoId）
		ReconnectVideoId.Reset();
		ReconnectAttempts = 0;
		return;
	}

	// 重连失败：继续重试或达到上限停止
	++ReconnectAttempts;
	if (UAV::Ffmpeg::ShouldRetryReconnect(ReconnectAttempts, MaxReconnectAttempts))
	{
		ScheduleReconnect();
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[UAVCameraStream] 重连次数达到上限（%d），停止推流：%s"), MaxReconnectAttempts, *Session->VideoId);
	Session->bStreaming = false;
	if (ActiveStreamVideoId == Session->VideoId)
	{
		ActiveStreamVideoId.Reset();
	}
	OnLiveStatusChanged.Broadcast(Session->VideoId);
	CancelReconnect();
}
