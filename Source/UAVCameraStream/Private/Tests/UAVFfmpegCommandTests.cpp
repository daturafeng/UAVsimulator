// UAVFfmpegCommand 推流命令构造自动化测试（任务 3.2）
#include "Misc/AutomationTest.h"
#include "UAVFfmpegCommand.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVFfmpegCommandTest, "UAV.Ffmpeg.Command", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVFfmpegCommandTest::RunTest(const FString& Parameters)
{
	// 自适应档位：1280x720@15fps / 2000kbps，无 scale（源尺寸一致）
	const UAV::CloudApi::FUAVVideoQualityParams Adaptive = UAV::CloudApi::GetVideoQualityParams(0);
	const FString Cmd0 = UAV::Ffmpeg::MakePushCommand(TEXT("C:/ffmpeg/bin/ffmpeg.exe"), Adaptive, TEXT("rtmp://127.0.0.1:1935/live/DOCK3TEST001-52-0-0"));
	TestTrue(TEXT("命令含 rawvideo 输入"), Cmd0.Contains(TEXT("-f rawvideo -pix_fmt bgra")));
	TestTrue(TEXT("命令含档位分辨率"), Cmd0.Contains(TEXT("-s 1280x720")));
	TestTrue(TEXT("命令含帧率"), Cmd0.Contains(TEXT("-r 15")));
	TestTrue(TEXT("命令含码率"), Cmd0.Contains(TEXT("-b:v 2000k")));
	TestTrue(TEXT("命令含 FLV 输出"), Cmd0.Contains(TEXT("-f flv")));
	TestTrue(TEXT("命令含 RTMP 地址"), Cmd0.Contains(TEXT("rtmp://127.0.0.1:1935/live/DOCK3TEST001-52-0-0")));
	TestTrue(TEXT("尺寸一致时不加 scale"), !Cmd0.Contains(TEXT("-vf scale")));

	// 高清档位：1920x1080 / 4000kbps 注入
	const UAV::CloudApi::FUAVVideoQualityParams Hd = UAV::CloudApi::GetVideoQualityParams(3);
	const FString Cmd3 = UAV::Ffmpeg::MakePushCommand(TEXT("ffmpeg"), Hd, TEXT("rtmp://srv/live/1"));
	TestTrue(TEXT("高清分辨率注入"), Cmd3.Contains(TEXT("-s 1920x1080")));
	TestTrue(TEXT("高清码率注入"), Cmd3.Contains(TEXT("-b:v 4000k")));

	// 源尺寸与档位不一致时追加 scale
	const FString CmdScale = UAV::Ffmpeg::MakePushCommand(TEXT("ffmpeg"), Hd, TEXT("rtmp://srv/live/1"), 1280, 720);
	TestTrue(TEXT("输入尺寸用源尺寸"), CmdScale.Contains(TEXT("-s 1280x720")));
	TestTrue(TEXT("追加 scale 到输出档位"), CmdScale.Contains(TEXT("-vf scale=1920x1080")));

	// 路径含空格时加引号
	const FString CmdQuoted = UAV::Ffmpeg::MakePushCommand(TEXT("C:/Program Files/ffmpeg/bin/ffmpeg.exe"), Adaptive, TEXT("rtmp://srv/live/1"));
	TestTrue(TEXT("路径加引号"), CmdQuoted.StartsWith(TEXT("\"C:/Program Files/ffmpeg/bin/ffmpeg.exe\"")));

	// 非法档位参数回退默认值
	UAV::CloudApi::FUAVVideoQualityParams Bad;
	const FString CmdBad = UAV::Ffmpeg::MakePushCommand(TEXT("ffmpeg"), Bad, TEXT("rtmp://srv/live/1"));
	TestTrue(TEXT("非法档位回退分辨率"), CmdBad.Contains(TEXT("-s 1280x720")));
	TestTrue(TEXT("非法档位回退码率"), CmdBad.Contains(TEXT("-b:v 2000k")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
