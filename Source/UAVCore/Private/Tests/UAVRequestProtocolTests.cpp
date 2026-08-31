// 设备主动 requests 协议自动化测试：topic/method 常量 + 统一报文头
#include "Misc/AutomationTest.h"
#include "UAVCloudApiTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVRequestProtocolTest, "UAV.CloudApi.RequestProtocol", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVRequestProtocolTest::RunTest(const FString& Parameters)
{
	using namespace UAV::CloudApi;

	TestEqual(TEXT("requests topic 模板"), TEXT("thing/product/{sn}/requests"), FString(kTopicRequestsTemplate));
	TestEqual(TEXT("requests_reply topic 模板"), TEXT("thing/product/{sn}/requests_reply"), FString(kTopicRequestsReplyTemplate));
	TestEqual(TEXT("config method"), TEXT("config"), FString(kRequestConfig));
	TestEqual(TEXT("bind status method"), TEXT("airport_bind_status"), FString(kRequestAirportBindStatus));
	TestEqual(TEXT("organization get method"), TEXT("airport_organization_get"), FString(kRequestAirportOrganizationGet));
	TestEqual(TEXT("organization bind method"), TEXT("airport_organization_bind"), FString(kRequestAirportOrganizationBind));
	TestEqual(TEXT("storage config method"), TEXT("storage_config_get"), FString(kRequestStorageConfigGet));
	TestEqual(TEXT("flight areas update service method"), TEXT("flight_areas_update"), FString(kMethodFlightAreasUpdate));
	TestEqual(TEXT("offline map update service method"), TEXT("offline_map_update"), FString(kMethodOfflineMapUpdate));
	TestEqual(TEXT("flighttask resource get request method"), TEXT("flighttask_resource_get"), FString(kRequestFlighttaskResourceGet));
	TestEqual(TEXT("flight areas get request method"), TEXT("flight_areas_get"), FString(kRequestFlightAreasGet));
	TestEqual(TEXT("offline map get request method"), TEXT("offline_map_get"), FString(kRequestOfflineMapGet));
	TestEqual(TEXT("flight areas sync progress event method"), TEXT("flight_areas_sync_progress"), FString(kEventFlightAreasSyncProgress));
	TestEqual(TEXT("offline map sync progress event method"), TEXT("offline_map_sync_progress"), FString(kEventOfflineMapSyncProgress));

	const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("config_type"), TEXT("json"));
	Data->SetStringField(TEXT("config_scope"), TEXT("product"));
	const TSharedRef<FJsonObject> Request = MakeRequestMessage(
		kRequestConfig, TEXT("DOCK3TEST001"), Data, TEXT("tid-request"), TEXT("bid-request"));

	TestEqual(TEXT("request.tid"), TEXT("tid-request"), Request->GetStringField(TEXT("tid")));
	TestEqual(TEXT("request.bid"), TEXT("bid-request"), Request->GetStringField(TEXT("bid")));
	TestEqual(TEXT("request.gateway"), TEXT("DOCK3TEST001"), Request->GetStringField(TEXT("gateway")));
	TestEqual(TEXT("request.method"), TEXT("config"), Request->GetStringField(TEXT("method")));
	TestTrue(TEXT("request.timestamp 为正数"), Request->GetNumberField(TEXT("timestamp")) > 0.0);
	const TSharedPtr<FJsonObject> RequestData = Request->GetObjectField(TEXT("data"));
	TestTrue(TEXT("request.data 存在"), RequestData.IsValid());
	if (RequestData.IsValid())
	{
		TestEqual(TEXT("config_type 透传"), TEXT("json"), RequestData->GetStringField(TEXT("config_type")));
		TestEqual(TEXT("config_scope 透传"), TEXT("product"), RequestData->GetStringField(TEXT("config_scope")));
	}

	const TSharedRef<FJsonObject> ResourceData = MakeShared<FJsonObject>();
	ResourceData->SetStringField(TEXT("flight_id"), TEXT("FLT-001"));
	const TSharedRef<FJsonObject> ResourceRequest = MakeRequestMessage(
		kRequestFlighttaskResourceGet, TEXT("DOCK3TEST001"), ResourceData, TEXT("tid-resource"), TEXT("bid-resource"));

	TestEqual(TEXT("resource request.tid"), TEXT("tid-resource"), ResourceRequest->GetStringField(TEXT("tid")));
	TestEqual(TEXT("resource request.bid"), TEXT("bid-resource"), ResourceRequest->GetStringField(TEXT("bid")));
	TestEqual(TEXT("resource request.gateway"), TEXT("DOCK3TEST001"), ResourceRequest->GetStringField(TEXT("gateway")));
	TestEqual(TEXT("resource request.method"), TEXT("flighttask_resource_get"), ResourceRequest->GetStringField(TEXT("method")));
	const TSharedPtr<FJsonObject> ResourceRequestData = ResourceRequest->GetObjectField(TEXT("data"));
	TestTrue(TEXT("resource request.data 存在"), ResourceRequestData.IsValid());
	if (ResourceRequestData.IsValid())
	{
		TestEqual(TEXT("flight_id 透传"), TEXT("FLT-001"), ResourceRequestData->GetStringField(TEXT("flight_id")));
	}

	const TSharedRef<FJsonObject> AutoIds = MakeRequestMessage(kRequestConfig, TEXT("DOCK3TEST001"), nullptr);
	TestTrue(TEXT("自动 tid 非空"), !AutoIds->GetStringField(TEXT("tid")).IsEmpty());
	TestTrue(TEXT("自动 bid 非空"), !AutoIds->GetStringField(TEXT("bid")).IsEmpty());
	TestTrue(TEXT("空 data 仍输出对象"), AutoIds->GetObjectField(TEXT("data")).IsValid());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
