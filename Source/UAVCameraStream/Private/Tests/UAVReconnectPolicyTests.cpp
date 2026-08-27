// 推流失败自动重连策略自动化测试（任务 1.2）
#include "Misc/AutomationTest.h"
#include "UAVFfmpegCommand.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVReconnectPolicyTest, "UAV.Ffmpeg.ReconnectPolicy", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVReconnectPolicyTest::RunTest(const FString& Parameters)
{
	// 禁用重连（MaxAttempts<=0）：任何已用次数都不再重试
	TestFalse(TEXT("上限 0 禁用重连"), UAV::Ffmpeg::ShouldRetryReconnect(0, 0));
	TestFalse(TEXT("上限 0 已用 0 不重试"), UAV::Ffmpeg::ShouldRetryReconnect(0, 0));
	TestFalse(TEXT("负数上限禁用重连"), UAV::Ffmpeg::ShouldRetryReconnect(0, -1));

	// 未达上限继续重试
	TestTrue(TEXT("0/3 继续重试"), UAV::Ffmpeg::ShouldRetryReconnect(0, 3));
	TestTrue(TEXT("2/3 继续重试"), UAV::Ffmpeg::ShouldRetryReconnect(2, 3));

	// 达到上限停止
	TestFalse(TEXT("3/3 停止重试"), UAV::Ffmpeg::ShouldRetryReconnect(3, 3));
	TestFalse(TEXT("4/3 停止重试"), UAV::Ffmpeg::ShouldRetryReconnect(4, 3));

	// 退避：attempt 0 为基准间隔
	TestEqual(TEXT("首次延迟为基准间隔"), UAV::Ffmpeg::NextReconnectDelaySeconds(0, 5.0, 30.0), 5.0);
	TestEqual(TEXT("第二次退避 2x"), UAV::Ffmpeg::NextReconnectDelaySeconds(1, 5.0, 30.0), 10.0);
	TestEqual(TEXT("第三次退避 4x"), UAV::Ffmpeg::NextReconnectDelaySeconds(2, 5.0, 30.0), 20.0);

	// 封顶
	TestEqual(TEXT("退避封顶于最大值"), UAV::Ffmpeg::NextReconnectDelaySeconds(3, 5.0, 30.0), 30.0);
	TestEqual(TEXT("深退避不溢出且封顶"), UAV::Ffmpeg::NextReconnectDelaySeconds(40, 5.0, 30.0), 30.0);

	// 非法参数：基准间隔 <=0 返回 0；负 attempt 返回 0
	TestEqual(TEXT("基准间隔 0 返回 0"), UAV::Ffmpeg::NextReconnectDelaySeconds(0, 0.0, 30.0), 0.0);
	TestEqual(TEXT("负 attempt 返回 0"), UAV::Ffmpeg::NextReconnectDelaySeconds(-1, 5.0, 30.0), 0.0);

	// 无封顶时退避仍受合理上限约束（不返回无穷/溢出值）
	const double Unlimited = UAV::Ffmpeg::NextReconnectDelaySeconds(60, 5.0, 0.0);
	TestTrue(TEXT("无封顶退避在合理范围内"), Unlimited > 0.0 && Unlimited <= 3600.0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
