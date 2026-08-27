// FFmpeg 推流命令构造（纯函数，便于单元测试）
#pragma once

#include "CoreMinimal.h"
#include "UAVCloudApiTypes.h"

namespace UAV::Ffmpeg
{
	/**
	 * 构造 FFmpeg rawvideo 推流命令（首期仅视频，无音频输入）。
	 * InSourceWidth/InSourceHeight 为输入帧（RenderTarget）尺寸；与档位不一致时自动追加 scale，
	 * 一致时输出与设计模板一致：-s {W}x{H} -r {fps} -b:v {kbps}k -f flv {url}。
	 * 路径或 URL 含空格时自动加引号包裹（CreateProc 无 shell 解析）。
	 */
	UAVCAMERASTREAM_API FString MakePushCommand(const FString& InFfmpegPath, const UAV::CloudApi::FUAVVideoQualityParams& InParams, const FString& InRtmpUrl, int32 InSourceWidth = 0, int32 InSourceHeight = 0);

	/**
	 * 是否应继续重连：已用重连次数小于上限（MaxAttempts<=0 表示禁用重连，返回 false）。
	 */
	UAVCAMERASTREAM_API bool ShouldRetryReconnect(int32 InAttemptsUsed, int32 InMaxAttempts);

	/**
	 * 下一次重连延迟（秒）：指数退避 delay = min(Base * 2^Attempt, Max)。
	 * Base<=0 时返回 0；Max<=0 时视为不封顶；Attempt 从 0 起。
	 */
	UAVCAMERASTREAM_API double NextReconnectDelaySeconds(int32 InAttempt, double InBaseSeconds, double InMaxSeconds);
}
