#include "UAVCameraStreamComponent.h"
#include "UAVCloudApiTypes.h"

#include "Serialization/JsonSerializer.h"

namespace
{
	/** 直播指令结果码（0=成功） */
	constexpr int32 kResultSuccess = 0;
	constexpr int32 kResultInvalidUrlType = 1;
	constexpr int32 kResultSessionNotFound = 2;
	constexpr int32 kResultInvalidParams = 3;
	constexpr int32 kResultUnknownMethod = 4;

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
	PrimaryComponentTick.bCanEverTick = false;
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

	// 同一 video_id 重复开启视为已存在（保持推流状态，不新建会话）
	if (FUAVLiveSession* Existing = FindSession(VideoId))
	{
		Existing->bStreaming = true;
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
	Session->bStreaming = false;
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
	UE_LOG(LogTemp, Log, TEXT("[UAVCameraStream] 镜头切换：%s -> %s"), *OldVideoId, *Session->VideoId);
	OnLiveStatusChanged.Broadcast(Session->VideoId);
	return kResultSuccess;
}

