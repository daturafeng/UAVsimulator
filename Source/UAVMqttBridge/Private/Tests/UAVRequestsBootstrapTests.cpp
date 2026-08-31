// 设备主动 requests 启动握手自动化测试：data 构建、响应关联、状态解析与失败保护
#include "Misc/AutomationTest.h"
#include "UAVMqttBridgeComponent.h"
#include "UAVCloudApiTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVRequestsDataBuilderTest, "UAV.MqttBridge.Requests.DataBuilders", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVRequestsDataBuilderTest::RunTest(const FString& Parameters)
{
	using namespace UAV::CloudApi;

	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> Config = Bridge->BuildConfigRequestData();
	TestEqual(TEXT("config_type=json"), TEXT("json"), Config->GetStringField(TEXT("config_type")));
	TestEqual(TEXT("config_scope=product"), TEXT("product"), Config->GetStringField(TEXT("config_scope")));

	const TSharedPtr<FJsonObject> BindStatus = Bridge->BuildAirportBindStatusRequestData();
	const TArray<TSharedPtr<FJsonValue>>* Devices = nullptr;
	TestTrue(TEXT("bind status devices 存在"), BindStatus->TryGetArrayField(TEXT("devices"), Devices));
	if (Devices)
	{
		TestEqual(TEXT("bind status 两台设备"), 2, Devices->Num());
		if (Devices->Num() == 2)
		{
			TestEqual(TEXT("机场 SN"), TEXT("DOCK3TEST001"), (*Devices)[0]->AsObject()->GetStringField(TEXT("sn")));
			TestEqual(TEXT("无人机 SN"), TEXT("1581F8HGXTEST001"), (*Devices)[1]->AsObject()->GetStringField(TEXT("sn")));
		}
	}

	Bridge->DeviceBindingCode = TEXT("BIND-CODE");
	Bridge->OrganizationId = TEXT("ORG-001");
	const TSharedPtr<FJsonObject> OrganizationGet = Bridge->BuildAirportOrganizationGetRequestData();
	TestEqual(TEXT("organization get 绑定码"), TEXT("BIND-CODE"), OrganizationGet->GetStringField(TEXT("device_binding_code")));
	TestEqual(TEXT("organization get 组织 ID"), TEXT("ORG-001"), OrganizationGet->GetStringField(TEXT("organization_id")));

	const TSharedPtr<FJsonObject> OrganizationBind = Bridge->BuildAirportOrganizationBindRequestData();
	const TArray<TSharedPtr<FJsonValue>>* BindDevices = nullptr;
	TestTrue(TEXT("bind_devices 存在"), OrganizationBind->TryGetArrayField(TEXT("bind_devices"), BindDevices));
	if (BindDevices && BindDevices->Num() == 2)
	{
		const TSharedPtr<FJsonObject> Dock = (*BindDevices)[0]->AsObject();
		const TSharedPtr<FJsonObject> Drone = (*BindDevices)[1]->AsObject();
		TestEqual(TEXT("机场 model key"), TEXT("3-3-0"), Dock->GetStringField(TEXT("device_model_key")));
		TestEqual(TEXT("无人机 model key"), TEXT("0-100-1"), Drone->GetStringField(TEXT("device_model_key")));
		TestEqual(TEXT("机场绑定码"), TEXT("BIND-CODE"), Dock->GetStringField(TEXT("device_binding_code")));
		TestEqual(TEXT("无人机组织 ID"), TEXT("ORG-001"), Drone->GetStringField(TEXT("organization_id")));
	}
	else
	{
		TestTrue(TEXT("bind_devices 数量为 2"), false);
	}

	const TSharedPtr<FJsonObject> Storage = Bridge->BuildStorageConfigGetRequestData();
	TestEqual(TEXT("storage module=0"), 0.0, Storage->GetNumberField(TEXT("module")));

	const TSharedPtr<FJsonObject> Tracked = Bridge->BuildTrackedRequestMessage(
		kRequestConfig, Config, TEXT("tid-config"), TEXT("bid-config"));
	TestTrue(TEXT("跟踪请求已组装"), Tracked.IsValid());
	TestEqual(TEXT("tracked tid"), TEXT("tid-config"), Tracked->GetStringField(TEXT("tid")));
	TestEqual(TEXT("tracked bid"), TEXT("bid-config"), Tracked->GetStringField(TEXT("bid")));
	TestEqual(TEXT("tracked gateway"), TEXT("DOCK3TEST001"), Tracked->GetStringField(TEXT("gateway")));
	TestTrue(TEXT("pending 含 bid-config"), Bridge->HasPendingRequest(TEXT("bid-config")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVRequestsReplyParsingTest, "UAV.MqttBridge.Requests.ReplyParsing", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVRequestsReplyParsingTest::RunTest(const FString& Parameters)
{
	using namespace UAV::CloudApi;

	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	Bridge->BuildTrackedRequestMessage(kRequestConfig, Bridge->BuildConfigRequestData(), TEXT("tid-config"), TEXT("bid-config"));
	const FString ConfigReply = TEXT("{\"tid\":\"tid-config\",\"bid\":\"bid-config\",\"method\":\"config\",\"timestamp\":1,\"data\":{\"ntp_server_host\":\"ntp.example.com\",\"app_id\":\"app-id\",\"app_key\":\"app-key\",\"app_license\":\"license\"}}");
	TestTrue(TEXT("config 响应成功"), Bridge->DispatchRequestsReplyMessage(ConfigReply));
	const FUAVProductConfigState Product = Bridge->GetProductConfigState();
	TestTrue(TEXT("产品配置有效"), Product.bValid);
	TestEqual(TEXT("产品配置 app_id"), TEXT("app-id"), Product.AppId);
	TestFalse(TEXT("config pending 已消费"), Bridge->HasPendingRequest(TEXT("bid-config")));

	Bridge->BuildTrackedRequestMessage(kRequestAirportBindStatus, Bridge->BuildAirportBindStatusRequestData(), TEXT("tid-bind-status"), TEXT("bid-bind-status"));
	const FString BindStatusReply = TEXT("{\"tid\":\"tid-bind-status\",\"bid\":\"bid-bind-status\",\"method\":\"airport_bind_status\",\"timestamp\":1,\"data\":{\"result\":0,\"output\":{\"bind_status\":[{\"sn\":\"DOCK3TEST001\",\"is_device_bind_organization\":false},{\"sn\":\"1581F8HGXTEST001\",\"is_device_bind_organization\":true,\"organization_id\":\"ORG-001\",\"organization_name\":\"测试组织\",\"device_callsign\":\"M4TD\"}]}}}");
	TestTrue(TEXT("bind status 响应成功"), Bridge->DispatchRequestsReplyMessage(BindStatusReply));
	const FUAVDeviceOrganizationState DockState = Bridge->GetDockOrganizationState();
	const FUAVDeviceOrganizationState DroneState = Bridge->GetDroneOrganizationState();
	TestTrue(TEXT("机场绑定状态已响应"), DockState.bHasResponse);
	TestFalse(TEXT("机场未绑定"), DockState.bBound);
	TestTrue(TEXT("无人机已绑定"), DroneState.bBound);
	TestEqual(TEXT("无人机组织名"), TEXT("测试组织"), DroneState.OrganizationName);

	Bridge->BuildTrackedRequestMessage(kRequestAirportOrganizationGet, Bridge->BuildAirportOrganizationGetRequestData(), TEXT("tid-org-get"), TEXT("bid-org-get"));
	const FString OrganizationGetReply = TEXT("{\"tid\":\"tid-org-get\",\"bid\":\"bid-org-get\",\"method\":\"airport_organization_get\",\"timestamp\":1,\"data\":{\"result\":0,\"output\":{\"organization_name\":\"测试组织\"}}}");
	TestTrue(TEXT("organization get 响应成功"), Bridge->DispatchRequestsReplyMessage(OrganizationGetReply));
	TestEqual(TEXT("记录组织名称"), TEXT("测试组织"), Bridge->GetOrganizationName());

	Bridge->BuildTrackedRequestMessage(kRequestAirportOrganizationBind, Bridge->BuildAirportOrganizationBindRequestData(), TEXT("tid-org-bind"), TEXT("bid-org-bind"));
	const FString OrganizationBindReply = TEXT("{\"tid\":\"tid-org-bind\",\"bid\":\"bid-org-bind\",\"method\":\"airport_organization_bind\",\"timestamp\":1,\"data\":{\"result\":0,\"output\":{\"err_infos\":[{\"sn\":\"DOCK3TEST001\",\"err_code\":0},{\"sn\":\"1581F8HGXTEST001\",\"err_code\":0}]}}}");
	TestTrue(TEXT("organization bind 响应成功"), Bridge->DispatchRequestsReplyMessage(OrganizationBindReply));
	TestTrue(TEXT("机场绑定成功"), Bridge->GetDockOrganizationState().bBound);
	TestTrue(TEXT("无人机绑定成功"), Bridge->GetDroneOrganizationState().bBound);

	Bridge->BuildTrackedRequestMessage(kRequestStorageConfigGet, Bridge->BuildStorageConfigGetRequestData(), TEXT("tid-storage"), TEXT("bid-storage"));
	const FString StorageReply = TEXT("{\"tid\":\"tid-storage\",\"bid\":\"bid-storage\",\"method\":\"storage_config_get\",\"timestamp\":1,\"data\":{\"result\":0,\"output\":{\"bucket\":\"media\",\"credentials\":{\"access_key_id\":\"ak\",\"secret_access_key\":\"sk\",\"security_token\":\"token\",\"expire\":3600},\"endpoint\":\"https://oss.example.com\",\"object_key_prefix\":\"media/files\",\"provider\":\"minio\",\"region\":\"cn-test\"}}}");
	TestTrue(TEXT("storage config 响应成功"), Bridge->DispatchRequestsReplyMessage(StorageReply));
	const FUAVStorageConfigState StorageState = Bridge->GetStorageConfigState();
	TestTrue(TEXT("对象存储配置有效"), StorageState.bValid);
	TestEqual(TEXT("对象存储 bucket"), TEXT("media"), StorageState.Bucket);
	TestEqual(TEXT("对象存储 provider"), TEXT("minio"), StorageState.Provider);
	TestTrue(TEXT("credentials 已记录"), StorageState.CredentialsJson.Contains(TEXT("access_key_id")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVRequestsReplyProtectionTest, "UAV.MqttBridge.Requests.ReplyProtection", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVRequestsReplyProtectionTest::RunTest(const FString& Parameters)
{
	using namespace UAV::CloudApi;

	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	// 先建立一份成功对象存储配置，后续失败响应不得覆盖。
	Bridge->BuildTrackedRequestMessage(kRequestStorageConfigGet, Bridge->BuildStorageConfigGetRequestData(), TEXT("tid-base"), TEXT("bid-base"));
	const FString BaseReply = TEXT("{\"tid\":\"tid-base\",\"bid\":\"bid-base\",\"method\":\"storage_config_get\",\"data\":{\"result\":0,\"output\":{\"bucket\":\"stable\",\"credentials\":{\"token\":\"secret\"},\"endpoint\":\"https://oss.example.com\",\"object_key_prefix\":\"stable/path\",\"provider\":\"minio\",\"region\":\"cn-test\"}}}");
	TestTrue(TEXT("基线 storage config 成功"), Bridge->DispatchRequestsReplyMessage(BaseReply));

	Bridge->BuildTrackedRequestMessage(kRequestStorageConfigGet, Bridge->BuildStorageConfigGetRequestData(), TEXT("tid-failed"), TEXT("bid-failed"));
	const FString FailedReply = TEXT("{\"tid\":\"tid-failed\",\"bid\":\"bid-failed\",\"method\":\"storage_config_get\",\"data\":{\"result\":7,\"output\":{\"bucket\":\"bad\"}}}");
	TestFalse(TEXT("失败 storage config 返回 false"), Bridge->DispatchRequestsReplyMessage(FailedReply));
	TestEqual(TEXT("失败响应保留旧 bucket"), TEXT("stable"), Bridge->GetStorageConfigState().Bucket);
	TestFalse(TEXT("已关联失败响应仍消费 pending"), Bridge->HasPendingRequest(TEXT("bid-failed")));

	Bridge->BuildTrackedRequestMessage(kRequestConfig, Bridge->BuildConfigRequestData(), TEXT("tid-protect"), TEXT("bid-protect"));
	const FString WrongTid = TEXT("{\"tid\":\"tid-wrong\",\"bid\":\"bid-protect\",\"method\":\"config\",\"data\":{\"ntp_server_host\":\"x\",\"app_id\":\"x\",\"app_key\":\"x\",\"app_license\":\"x\"}}");
	TestFalse(TEXT("tid 错配被忽略"), Bridge->DispatchRequestsReplyMessage(WrongTid));
	TestTrue(TEXT("tid 错配不消费 pending"), Bridge->HasPendingRequest(TEXT("bid-protect")));

	const FString WrongMethod = TEXT("{\"tid\":\"tid-protect\",\"bid\":\"bid-protect\",\"method\":\"storage_config_get\",\"data\":{\"result\":0,\"output\":{}}}");
	TestFalse(TEXT("method 错配被忽略"), Bridge->DispatchRequestsReplyMessage(WrongMethod));
	TestTrue(TEXT("method 错配不消费 pending"), Bridge->HasPendingRequest(TEXT("bid-protect")));

	const FString UnknownBid = TEXT("{\"tid\":\"tid-protect\",\"bid\":\"bid-unknown\",\"method\":\"config\",\"data\":{}}");
	TestFalse(TEXT("未知 bid 被忽略"), Bridge->DispatchRequestsReplyMessage(UnknownBid));
	TestTrue(TEXT("未知 bid 不影响原 pending"), Bridge->HasPendingRequest(TEXT("bid-protect")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
