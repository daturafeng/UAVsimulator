// OTA 固件升级自动化测试：指令参数校验 + 回执结构 + 进度事件序列 + 固件版本 state
#include "Misc/AutomationTest.h"
#include "UAVMqttBridgeComponent.h"
#include "UAVFlightControlComponent.h"
#include "UAVCloudApiTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** 合法设备项（sn=机场，product_version 格式 xx.xx.xxxx，升级类型 2=NORMAL_UPGRADE） */
	const FString kValidDockDeviceJson = TEXT("{\"sn\":\"DOCK3TEST001\",\"product_version\":\"03.02.0001\",\"file_url\":\"http://example.com/dock.bin\",\"md5\":\"abc123\",\"file_size\":1024,\"firmware_upgrade_type\":2,\"file_name\":\"dock.bin\"}");

	/** 合法设备项（sn=无人机，升级类型 3=CONSISTENT_UPGRADE） */
	const FString kValidDroneDeviceJson = TEXT("{\"sn\":\"1581F8HGXTEST001\",\"product_version\":\"03.02.0001\",\"file_url\":\"http://example.com/drone.bin\",\"md5\":\"def456\",\"file_size\":2048,\"firmware_upgrade_type\":3,\"file_name\":\"drone.bin\"}");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVOtaCommandValidationTest, "UAV.MqttBridge.Ota.CommandValidation", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVOtaCommandValidationTest::RunTest(const FString& Parameters)
{
	using namespace UAV::FlightControlResult;

	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	// 单设备升级成功（机场）
	TestEqual(TEXT("单设备 ota_create 成功"), Success, Bridge->HandleOtaCreate(TEXT("ota_create"), FString::Printf(TEXT("{\"devices\":[%s]}"), *kValidDockDeviceJson)));
	// 双设备升级成功（机场 + 无人机）
	TestEqual(TEXT("双设备 ota_create 成功"), Success, Bridge->HandleOtaCreate(TEXT("ota_create"), FString::Printf(TEXT("{\"devices\":[%s,%s]}"), *kValidDockDeviceJson, *kValidDroneDeviceJson)));
	// 未知方法
	TestEqual(TEXT("未知方法失败"), UnknownMethod, Bridge->HandleOtaCreate(TEXT("ota_progress"), TEXT("")));

	// devices 缺失 / 为空 / 数量越界
	TestEqual(TEXT("devices 缺失失败"), InvalidParams, Bridge->HandleOtaCreate(TEXT("ota_create"), TEXT("{}")));
	TestEqual(TEXT("devices 为空失败"), InvalidParams, Bridge->HandleOtaCreate(TEXT("ota_create"), TEXT("{\"devices\":[]}")));
	TestEqual(TEXT("devices 三个失败"), InvalidParams, Bridge->HandleOtaCreate(TEXT("ota_create"), FString::Printf(TEXT("{\"devices\":[%s,%s,%s]}"), *kValidDockDeviceJson, *kValidDroneDeviceJson, *kValidDockDeviceJson)));

	// 设备项字段缺失/非法
	TestEqual(TEXT("缺 sn 失败"), InvalidParams, Bridge->HandleOtaCreate(TEXT("ota_create"), TEXT("{\"devices\":[{\"product_version\":\"03.02.0001\",\"file_url\":\"u\",\"md5\":\"m\",\"file_size\":1,\"firmware_upgrade_type\":2,\"file_name\":\"f\"}]}")));
	TestEqual(TEXT("缺 product_version 失败"), InvalidParams, Bridge->HandleOtaCreate(TEXT("ota_create"), TEXT("{\"devices\":[{\"sn\":\"DOCK3TEST001\",\"file_url\":\"u\",\"md5\":\"m\",\"file_size\":1,\"firmware_upgrade_type\":2,\"file_name\":\"f\"}]}")));
	TestEqual(TEXT("缺 file_url 失败"), InvalidParams, Bridge->HandleOtaCreate(TEXT("ota_create"), TEXT("{\"devices\":[{\"sn\":\"DOCK3TEST001\",\"product_version\":\"03.02.0001\",\"md5\":\"m\",\"file_size\":1,\"firmware_upgrade_type\":2,\"file_name\":\"f\"}]}")));
	TestEqual(TEXT("缺 md5 失败"), InvalidParams, Bridge->HandleOtaCreate(TEXT("ota_create"), TEXT("{\"devices\":[{\"sn\":\"DOCK3TEST001\",\"product_version\":\"03.02.0001\",\"file_url\":\"u\",\"file_size\":1,\"firmware_upgrade_type\":2,\"file_name\":\"f\"}]}")));
	TestEqual(TEXT("缺 file_size 失败"), InvalidParams, Bridge->HandleOtaCreate(TEXT("ota_create"), TEXT("{\"devices\":[{\"sn\":\"DOCK3TEST001\",\"product_version\":\"03.02.0001\",\"file_url\":\"u\",\"md5\":\"m\",\"firmware_upgrade_type\":2,\"file_name\":\"f\"}]}")));
	TestEqual(TEXT("缺 file_name 失败"), InvalidParams, Bridge->HandleOtaCreate(TEXT("ota_create"), TEXT("{\"devices\":[{\"sn\":\"DOCK3TEST001\",\"product_version\":\"03.02.0001\",\"file_url\":\"u\",\"md5\":\"m\",\"file_size\":1,\"firmware_upgrade_type\":2}]}")));

	// product_version 格式非法 / 升级类型越界 / 类型不符 / 非法 JSON
	TestEqual(TEXT("版本格式非法失败"), InvalidParams, Bridge->HandleOtaCreate(TEXT("ota_create"), TEXT("{\"devices\":[{\"sn\":\"DOCK3TEST001\",\"product_version\":\"3.2.1\",\"file_url\":\"u\",\"md5\":\"m\",\"file_size\":1,\"firmware_upgrade_type\":2,\"file_name\":\"f\"}]}")));
	TestEqual(TEXT("升级类型 4 越界失败"), InvalidParams, Bridge->HandleOtaCreate(TEXT("ota_create"), TEXT("{\"devices\":[{\"sn\":\"DOCK3TEST001\",\"product_version\":\"03.02.0001\",\"file_url\":\"u\",\"md5\":\"m\",\"file_size\":1,\"firmware_upgrade_type\":4,\"file_name\":\"f\"}]}")));
	TestEqual(TEXT("file_size 非数值失败"), InvalidParams, Bridge->HandleOtaCreate(TEXT("ota_create"), TEXT("{\"devices\":[{\"sn\":\"DOCK3TEST001\",\"product_version\":\"03.02.0001\",\"file_url\":\"u\",\"md5\":\"m\",\"file_size\":\"big\",\"firmware_upgrade_type\":2,\"file_name\":\"f\"}]}")));
	TestEqual(TEXT("非法 JSON 失败"), InvalidParams, Bridge->HandleOtaCreate(TEXT("ota_create"), TEXT("{broken")));
	TestEqual(TEXT("空 data 失败"), InvalidParams, Bridge->HandleOtaCreate(TEXT("ota_create"), TEXT("")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVOtaProgressEventTest, "UAV.MqttBridge.Ota.ProgressEvent", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVOtaProgressEventTest::RunTest(const FString& Parameters)
{
	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	// sent 阶段：percent=0、current_step=1、rate=0（对齐 OtaProgressStatusEnum.SENT）
	const TSharedPtr<FJsonObject> Sent = Bridge->BuildOtaProgressEventData(TEXT("sent"), 0, 1, 0);
	TestTrue(TEXT("sent 事件 data 已组装"), Sent.IsValid());
	if (Sent.IsValid())
	{
		TestEqual(TEXT("sent result=0"), 0.0, Sent->GetNumberField(TEXT("result")));
		const TSharedPtr<FJsonObject> Output = Sent->GetObjectField(TEXT("output"));
		TestTrue(TEXT("output 对象存在"), Output.IsValid());
		if (Output.IsValid())
		{
			TestEqual(TEXT("output.status=sent"), TEXT("sent"), Output->GetStringField(TEXT("status")));
			const TSharedPtr<FJsonObject> Progress = Output->GetObjectField(TEXT("progress"));
			TestTrue(TEXT("progress 对象存在"), Progress.IsValid());
			if (Progress.IsValid())
			{
				TestEqual(TEXT("percent=0"), 0.0, Progress->GetNumberField(TEXT("percent")));
				TestEqual(TEXT("current_step=1"), 1.0, Progress->GetNumberField(TEXT("current_step")));
			}
			TestEqual(TEXT("ext.rate=0"), 0.0, Output->GetObjectField(TEXT("ext"))->GetNumberField(TEXT("rate")));
		}
	}

	// in_progress 阶段：percent=50
	const TSharedPtr<FJsonObject> InProgress = Bridge->BuildOtaProgressEventData(TEXT("in_progress"), 50, 1, 0);
	TestEqual(TEXT("in_progress status"), TEXT("in_progress"), InProgress->GetObjectField(TEXT("output"))->GetStringField(TEXT("status")));
	TestEqual(TEXT("in_progress percent=50"), 50.0, InProgress->GetObjectField(TEXT("output"))->GetObjectField(TEXT("progress"))->GetNumberField(TEXT("percent")));

	// ok 阶段：percent=100、current_step=2（对齐 OtaProgressStepEnum：2=UPGRADING）
	const TSharedPtr<FJsonObject> Ok = Bridge->BuildOtaProgressEventData(TEXT("ok"), 100, 2, 0);
	TestEqual(TEXT("ok status"), TEXT("ok"), Ok->GetObjectField(TEXT("output"))->GetStringField(TEXT("status")));
	TestEqual(TEXT("ok percent=100"), 100.0, Ok->GetObjectField(TEXT("output"))->GetObjectField(TEXT("progress"))->GetNumberField(TEXT("percent")));
	TestEqual(TEXT("ok current_step=2"), 2.0, Ok->GetObjectField(TEXT("output"))->GetObjectField(TEXT("progress"))->GetNumberField(TEXT("current_step")));

	// services_reply 成功回执带 output.status="sent"（对齐 ServicesReplyData<OtaCreateResponse>）
	const TSharedRef<FJsonObject> Output = MakeShared<FJsonObject>();
	Output->SetStringField(TEXT("status"), TEXT("sent"));
	const TSharedRef<FJsonObject> Reply = UAV::CloudApi::MakeServicesReply(TEXT("ota_create"), TEXT("tid-1"), TEXT("bid-1"), 0, Output);
	const TSharedPtr<FJsonObject> ReplyData = Reply->GetObjectField(TEXT("data"));
	TestEqual(TEXT("回执 result=0"), 0.0, ReplyData->GetNumberField(TEXT("result")));
	TestEqual(TEXT("回执 output.status=sent"), TEXT("sent"), ReplyData->GetObjectField(TEXT("output"))->GetStringField(TEXT("status")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVOtaFirmwareVersionStateTest, "UAV.MqttBridge.Ota.FirmwareVersionState", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVOtaFirmwareVersionStateTest::RunTest(const FString& Parameters)
{
	using namespace UAV::FlightControlResult;

	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	// 默认固件版本 state：机场含 compatible_status/firmware_upgrade_status，无人机与载荷仅 firmware_version
	const TSharedPtr<FJsonObject> DockDefault = Bridge->BuildDockFirmwareVersionData();
	TestTrue(TEXT("机场固件版本 data 已组装"), DockDefault.IsValid());
	if (DockDefault.IsValid())
	{
		TestEqual(TEXT("默认机场版本 03.01.0000"), TEXT("03.01.0000"), DockDefault->GetStringField(TEXT("firmware_version")));
		TestFalse(TEXT("默认无需一致性升级"), DockDefault->GetBoolField(TEXT("compatible_status")));
		TestFalse(TEXT("默认非升级中"), DockDefault->GetBoolField(TEXT("firmware_upgrade_status")));
	}
	const TSharedPtr<FJsonObject> DroneDefault = Bridge->BuildDroneFirmwareVersionData();
	TestEqual(TEXT("默认无人机版本 03.01.0000"), TEXT("03.01.0000"), DroneDefault->GetStringField(TEXT("firmware_version")));
	const TSharedPtr<FJsonObject> PayloadDefault = Bridge->BuildPayloadFirmwareVersionData();
	TestEqual(TEXT("载荷索引键为 52-0-0"), TEXT("03.01.0000"), PayloadDefault->GetObjectField(TEXT("52-0-0"))->GetStringField(TEXT("firmware_version")));

	// ota_create 成功后：进入升级中（firmware_upgrade_status=true），版本仍为默认
	TestEqual(TEXT("升级指令成功"), Success, Bridge->HandleOtaCreate(TEXT("ota_create"), FString::Printf(TEXT("{\"devices\":[%s,%s]}"), *kValidDockDeviceJson, *kValidDroneDeviceJson)));
	TestTrue(TEXT("升级中 firmware_upgrade_status=true"), Bridge->BuildDockFirmwareVersionData()->GetBoolField(TEXT("firmware_upgrade_status")));
	TestEqual(TEXT("升级中版本仍为默认"), TEXT("03.01.0000"), Bridge->BuildDockFirmwareVersionData()->GetStringField(TEXT("firmware_version")));

	// 完成升级（ok 事件后）：目标版本落地、恢复非升级状态
	Bridge->CompleteOtaUpgrade();
	TestEqual(TEXT("升级后机场版本=目标版本"), TEXT("03.02.0001"), Bridge->BuildDockFirmwareVersionData()->GetStringField(TEXT("firmware_version")));
	TestFalse(TEXT("升级后 firmware_upgrade_status=false"), Bridge->BuildDockFirmwareVersionData()->GetBoolField(TEXT("firmware_upgrade_status")));
	TestEqual(TEXT("升级后无人机版本=目标版本"), TEXT("03.02.0001"), Bridge->BuildDroneFirmwareVersionData()->GetStringField(TEXT("firmware_version")));
	TestEqual(TEXT("升级后载荷版本=目标版本"), TEXT("03.02.0001"), Bridge->BuildPayloadFirmwareVersionData()->GetObjectField(TEXT("52-0-0"))->GetStringField(TEXT("firmware_version")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
