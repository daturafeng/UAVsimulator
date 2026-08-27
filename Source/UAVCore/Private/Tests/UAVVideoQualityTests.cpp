// UAVCloudApiTypes 视频清晰度档位自动化测试（任务 1.2）
#include "Misc/AutomationTest.h"
#include "UAVCloudApiTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVVideoQualityParamsTest, "UAV.CloudApi.VideoQuality", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVVideoQualityParamsTest::RunTest(const FString& Parameters)
{
	// 0=自适应：1280x720@15fps / 2000kbps
	const UAV::CloudApi::FUAVVideoQualityParams Adaptive = UAV::CloudApi::GetVideoQualityParams(0);
	TestEqual(TEXT("自适应-宽"), Adaptive.Width, 1280);
	TestEqual(TEXT("自适应-高"), Adaptive.Height, 720);
	TestEqual(TEXT("自适应-码率"), Adaptive.BitrateKbps, 2000);
	TestEqual(TEXT("自适应-帧率"), Adaptive.Fps, 15);

	// 1=流畅：640x360@15fps / 800kbps
	const UAV::CloudApi::FUAVVideoQualityParams Smooth = UAV::CloudApi::GetVideoQualityParams(1);
	TestEqual(TEXT("流畅-宽"), Smooth.Width, 640);
	TestEqual(TEXT("流畅-高"), Smooth.Height, 360);
	TestEqual(TEXT("流畅-码率"), Smooth.BitrateKbps, 800);

	// 2=标清：1280x720@15fps / 2000kbps
	const UAV::CloudApi::FUAVVideoQualityParams Sd = UAV::CloudApi::GetVideoQualityParams(2);
	TestEqual(TEXT("标清-宽"), Sd.Width, 1280);
	TestEqual(TEXT("标清-高"), Sd.Height, 720);
	TestEqual(TEXT("标清-码率"), Sd.BitrateKbps, 2000);

	// 3=高清：1920x1080@15fps / 4000kbps
	const UAV::CloudApi::FUAVVideoQualityParams Hd = UAV::CloudApi::GetVideoQualityParams(3);
	TestEqual(TEXT("高清-宽"), Hd.Width, 1920);
	TestEqual(TEXT("高清-高"), Hd.Height, 1080);
	TestEqual(TEXT("高清-码率"), Hd.BitrateKbps, 4000);

	// 4=超清：1920x1080@15fps / 6000kbps
	const UAV::CloudApi::FUAVVideoQualityParams Uhd = UAV::CloudApi::GetVideoQualityParams(4);
	TestEqual(TEXT("超清-宽"), Uhd.Width, 1920);
	TestEqual(TEXT("超清-高"), Uhd.Height, 1080);
	TestEqual(TEXT("超清-码率"), Uhd.BitrateKbps, 6000);

	// 确定性：同档位两次调用返回相同参数
	const UAV::CloudApi::FUAVVideoQualityParams AdaptiveAgain = UAV::CloudApi::GetVideoQualityParams(0);
	TestEqual(TEXT("确定性-宽"), AdaptiveAgain.Width, Adaptive.Width);
	TestEqual(TEXT("确定性-码率"), AdaptiveAgain.BitrateKbps, Adaptive.BitrateKbps);

	// 非法档位（-1/5）回退自适应
	const UAV::CloudApi::FUAVVideoQualityParams Negative = UAV::CloudApi::GetVideoQualityParams(-1);
	TestEqual(TEXT("非法-负档位宽"), Negative.Width, Adaptive.Width);
	const UAV::CloudApi::FUAVVideoQualityParams OutOfRange = UAV::CloudApi::GetVideoQualityParams(5);
	TestEqual(TEXT("非法-超档位宽"), OutOfRange.Width, Adaptive.Width);
	TestEqual(TEXT("非法-超档位码率"), OutOfRange.BitrateKbps, Adaptive.BitrateKbps);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
