// FFmpeg 推流命令构造实现
#include "UAVFfmpegCommand.h"

namespace UAV::Ffmpeg
{
	namespace
	{
		constexpr double kMaxReasonableDelaySeconds = 3600.0;
	}

	FString MakePushCommand(const FString& InFfmpegPath, const UAV::CloudApi::FUAVVideoQualityParams& InParams, const FString& InRtmpUrl, int32 InSourceWidth, int32 InSourceHeight)
	{
		// ffmpeg 路径含空格时加引号（CreateProc 直接解析命令行，无 shell）
		FString Ffmpeg = InFfmpegPath;
		if (Ffmpeg.Contains(TEXT(" ")) && !Ffmpeg.StartsWith(TEXT("\"")))
		{
			Ffmpeg = FString::Printf(TEXT("\"%s\""), *Ffmpeg);
		}

		FString Url = InRtmpUrl;
		if (Url.Contains(TEXT(" ")))
		{
			Url = FString::Printf(TEXT("\"%s\""), *Url);
		}

		// 档位参数兜底（非法输入退化为 1280x720@15/2000kbps）
		const int32 OutW = InParams.Width > 0 ? InParams.Width : 1280;
		const int32 OutH = InParams.Height > 0 ? InParams.Height : 720;
		const int32 Fps = InParams.Fps > 0 ? InParams.Fps : 15;
		const int32 Kbps = InParams.BitrateKbps > 0 ? InParams.BitrateKbps : 2000;
		const int32 SrcW = InSourceWidth > 0 ? InSourceWidth : OutW;
		const int32 SrcH = InSourceHeight > 0 ? InSourceHeight : OutH;

		// 输入尺寸与输出档位不一致时由 FFmpeg 缩放
		FString ScaleArgs;
		if (SrcW != OutW || SrcH != OutH)
		{
			ScaleArgs = FString::Printf(TEXT(" -vf scale=%dx%d"), OutW, OutH);
		}

		return FString::Printf(TEXT("%s -f rawvideo -pix_fmt bgra -s %dx%d -r %d -i - -c:v libx264 -preset ultrafast -tune zerolatency -b:v %dk%s -f flv %s"),
			*Ffmpeg, SrcW, SrcH, Fps, Kbps, *ScaleArgs, *Url);
	}

	bool ShouldRetryReconnect(const int32 InAttemptsUsed, const int32 InMaxAttempts)
	{
		return InMaxAttempts > 0 && InAttemptsUsed < InMaxAttempts;
	}

	double NextReconnectDelaySeconds(const int32 InAttempt, const double InBaseSeconds, const double InMaxSeconds)
	{
		if (InBaseSeconds <= 0.0 || InAttempt < 0)
		{
			return 0.0;
		}

		// 指数退避，防止 2^N 溢出：超过封顶直接取封顶
		const int32 Shift = InAttempt > 30 ? 30 : InAttempt;
		const double Raw = InBaseSeconds * static_cast<double>(1LL << Shift);
		if (InMaxSeconds > 0.0)
		{
			return FMath::Min(Raw, InMaxSeconds);
		}
		return FMath::Min(Raw, kMaxReasonableDelaySeconds);
	}
}
