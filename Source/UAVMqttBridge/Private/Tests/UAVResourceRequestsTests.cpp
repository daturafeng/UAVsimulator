// 云端资源 requests 自动化测试：请求 data、三类响应、状态保护、同步事件与超时清理
#include "Misc/AutomationTest.h"
#include "UAVCloudApiTypes.h"
#include "UAVMqttBridgeComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVResourceRequestDataTest, "UAV.MqttBridge.ResourceRequests.Data", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVResourceRequestDataTest::RunTest(const FString& Parameters)
{
	using namespace UAV::CloudApi;

	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	const TSharedPtr<FJsonObject> Flighttask = Bridge->BuildFlighttaskResourceGetRequestData(TEXT("FLT-001"));
	TestTrue(TEXT("航线资源 data 有效"), Flighttask.IsValid());
	TestEqual(TEXT("flight_id 透传"), TEXT("FLT-001"), Flighttask->GetStringField(TEXT("flight_id")));
	TestFalse(TEXT("空 flight_id 拒绝"), Bridge->BuildFlighttaskResourceGetRequestData(FString()).IsValid());
	TestTrue(TEXT("空 flight_id 不发布"), Bridge->PublishFlighttaskResourceRequest(FString()).IsEmpty());
	TestEqual(TEXT("非法请求不创建 pending"), 0, Bridge->GetPendingRequestCount());

	const TSharedPtr<FJsonObject> EmptyData = Bridge->BuildEmptyResourceRequestData();
	TestTrue(TEXT("空资源 data 有效"), EmptyData.IsValid());
	TestEqual(TEXT("空资源 data 无字段"), 0, EmptyData->Values.Num());

	const TSharedPtr<FJsonObject> Tracked = Bridge->BuildTrackedRequestMessage(
		kRequestFlighttaskResourceGet, Flighttask, TEXT("tid-resource"), TEXT("bid-resource"), TEXT("FLT-001"));
	TestTrue(TEXT("航线资源请求已组装"), Tracked.IsValid());
	TestEqual(TEXT("航线请求 gateway"), TEXT("DOCK3TEST001"), Tracked->GetStringField(TEXT("gateway")));
	TestEqual(TEXT("航线请求 method"), TEXT("flighttask_resource_get"), Tracked->GetStringField(TEXT("method")));
	TestEqual(TEXT("航线请求 data.flight_id"), TEXT("FLT-001"), Tracked->GetObjectField(TEXT("data"))->GetStringField(TEXT("flight_id")));
	TestTrue(TEXT("航线请求进入 pending"), Bridge->HasPendingRequest(TEXT("bid-resource")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVResourceReplySuccessTest, "UAV.MqttBridge.ResourceRequests.Success", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVResourceReplySuccessTest::RunTest(const FString& Parameters)
{
	using namespace UAV::CloudApi;

	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	Bridge->BuildTrackedRequestMessage(kRequestFlighttaskResourceGet,
		Bridge->BuildFlighttaskResourceGetRequestData(TEXT("FLT-001")), TEXT("tid-wayline"), TEXT("bid-wayline"), TEXT("FLT-001"));
	const FString WaylineReply = TEXT("{\"tid\":\"tid-wayline\",\"bid\":\"bid-wayline\",\"method\":\"flighttask_resource_get\",\"data\":{\"result\":0,\"output\":{\"file\":{\"url\":\"https://files.example.com/route.kmz\",\"fingerprint\":\"sha256-route\"}}}}");
	TestTrue(TEXT("航线资源响应成功"), Bridge->DispatchRequestsReplyMessage(WaylineReply));
	const FUAVFlighttaskResourceState Wayline = Bridge->GetFlighttaskResourceState();
	TestTrue(TEXT("航线资源状态有效"), Wayline.bValid);
	TestEqual(TEXT("航线资源 flight_id"), TEXT("FLT-001"), Wayline.FlightId);
	TestEqual(TEXT("航线资源 URL"), TEXT("https://files.example.com/route.kmz"), Wayline.Url);
	TestEqual(TEXT("航线资源 fingerprint"), TEXT("sha256-route"), Wayline.Fingerprint);

	Bridge->BuildTrackedRequestMessage(kRequestFlightAreasGet, Bridge->BuildEmptyResourceRequestData(),
		TEXT("tid-area"), TEXT("bid-area"));
	const FString AreaReply = TEXT("{\"tid\":\"tid-area\",\"bid\":\"bid-area\",\"method\":\"flight_areas_get\",\"data\":{\"result\":0,\"output\":{\"files\":[{\"name\":\"geofence_0123456789abcdef0123456789abcdef.json\",\"url\":\"https://files.example.com/geofence.json\",\"checksum\":\"area-sha256\",\"size\":2048}]}}}");
	TestTrue(TEXT("飞行区域响应成功"), Bridge->DispatchRequestsReplyMessage(AreaReply));
	const FUAVFlightAreasState Areas = Bridge->GetFlightAreasState();
	TestTrue(TEXT("飞行区域状态有效"), Areas.bValid);
	TestEqual(TEXT("飞行区域文件数量"), 1, Areas.Files.Num());
	if (Areas.Files.Num() == 1)
	{
		TestEqual(TEXT("飞行区域文件名"), TEXT("geofence_0123456789abcdef0123456789abcdef.json"), Areas.Files[0].Name);
		TestEqual(TEXT("飞行区域文件大小"), static_cast<int64>(2048), Areas.Files[0].Size);
	}

	Bridge->BuildTrackedRequestMessage(kRequestOfflineMapGet, Bridge->BuildEmptyResourceRequestData(),
		TEXT("tid-map"), TEXT("bid-map"));
	const FString MapReply = TEXT("{\"tid\":\"tid-map\",\"bid\":\"bid-map\",\"method\":\"offline_map_get\",\"data\":{\"result\":0,\"output\":{\"offline_map_enable\":true,\"files\":[{\"name\":\"offline_map_full_v1_20260831.rocksdb.zip\",\"url\":\"https://files.example.com/map.zip\",\"checksum\":\"map-sha256\",\"size\":4096}]}}}");
	TestTrue(TEXT("离线地图响应成功"), Bridge->DispatchRequestsReplyMessage(MapReply));
	const FUAVOfflineMapState Map = Bridge->GetOfflineMapState();
	TestTrue(TEXT("离线地图状态有效"), Map.bValid);
	TestTrue(TEXT("离线地图已启用"), Map.bOfflineMapEnabled);
	TestEqual(TEXT("离线地图文件数量"), 1, Map.Files.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVResourceReplyProtectionTest, "UAV.MqttBridge.ResourceRequests.Protection", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVResourceReplyProtectionTest::RunTest(const FString& Parameters)
{
	using namespace UAV::CloudApi;

	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	Bridge->BuildTrackedRequestMessage(kRequestFlightAreasGet, Bridge->BuildEmptyResourceRequestData(), TEXT("tid-base-area"), TEXT("bid-base-area"));
	const FString BaseAreaReply = TEXT("{\"tid\":\"tid-base-area\",\"bid\":\"bid-base-area\",\"method\":\"flight_areas_get\",\"data\":{\"result\":0,\"output\":{\"files\":[{\"name\":\"geofence_0123456789abcdef0123456789abcdef.json\",\"url\":\"https://files.example.com/a.json\",\"checksum\":\"stable-area\",\"size\":1}]}}}");
	TestTrue(TEXT("飞行区域基线成功"), Bridge->DispatchRequestsReplyMessage(BaseAreaReply));

	Bridge->BuildTrackedRequestMessage(kRequestFlightAreasGet, Bridge->BuildEmptyResourceRequestData(), TEXT("tid-bad-area"), TEXT("bid-bad-area"));
	const FString BadAreaReply = TEXT("{\"tid\":\"tid-bad-area\",\"bid\":\"bid-bad-area\",\"method\":\"flight_areas_get\",\"data\":{\"result\":0,\"output\":{\"files\":[{\"name\":\"geofence_bad.json\",\"url\":\"u\",\"checksum\":\"c\",\"size\":1.5}]}}}");
	TestFalse(TEXT("非法飞行区域响应失败"), Bridge->DispatchRequestsReplyMessage(BadAreaReply));
	TestEqual(TEXT("非法响应保留旧 checksum"), TEXT("stable-area"), Bridge->GetFlightAreasState().Files[0].Checksum);

	Bridge->BuildTrackedRequestMessage(kRequestFlightAreasGet, Bridge->BuildEmptyResourceRequestData(), TEXT("tid-empty-area"), TEXT("bid-empty-area"));
	const FString EmptyAreaReply = TEXT("{\"tid\":\"tid-empty-area\",\"bid\":\"bid-empty-area\",\"method\":\"flight_areas_get\",\"data\":{\"result\":0,\"output\":{\"files\":[]}}}");
	TestTrue(TEXT("飞行区域空列表成功"), Bridge->DispatchRequestsReplyMessage(EmptyAreaReply));
	TestEqual(TEXT("飞行区域空列表已落地"), 0, Bridge->GetFlightAreasState().Files.Num());

	Bridge->BuildTrackedRequestMessage(kRequestOfflineMapGet, Bridge->BuildEmptyResourceRequestData(), TEXT("tid-off-map"), TEXT("bid-off-map"));
	const FString DisabledMapReply = TEXT("{\"tid\":\"tid-off-map\",\"bid\":\"bid-off-map\",\"method\":\"offline_map_get\",\"data\":{\"result\":0,\"output\":{\"offline_map_enable\":false,\"files\":[]}}}");
	TestTrue(TEXT("关闭离线地图空列表成功"), Bridge->DispatchRequestsReplyMessage(DisabledMapReply));
	TestFalse(TEXT("离线地图记录为关闭"), Bridge->GetOfflineMapState().bOfflineMapEnabled);

	Bridge->BuildTrackedRequestMessage(kRequestOfflineMapGet, Bridge->BuildEmptyResourceRequestData(), TEXT("tid-bad-map"), TEXT("bid-bad-map"));
	const FString BadMapReply = TEXT("{\"tid\":\"tid-bad-map\",\"bid\":\"bid-bad-map\",\"method\":\"offline_map_get\",\"data\":{\"result\":0,\"output\":{\"files\":[]}}}");
	TestFalse(TEXT("缺 offline_map_enable 失败"), Bridge->DispatchRequestsReplyMessage(BadMapReply));
	TestTrue(TEXT("失败后离线地图状态仍有效"), Bridge->GetOfflineMapState().bValid);
	TestFalse(TEXT("失败后仍保持关闭"), Bridge->GetOfflineMapState().bOfflineMapEnabled);

	Bridge->BuildTrackedRequestMessage(kRequestFlighttaskResourceGet,
		Bridge->BuildFlighttaskResourceGetRequestData(TEXT("FLT-OLD")), TEXT("tid-old-wayline"), TEXT("bid-old-wayline"), TEXT("FLT-OLD"));
	const FString GoodWayline = TEXT("{\"tid\":\"tid-old-wayline\",\"bid\":\"bid-old-wayline\",\"method\":\"flighttask_resource_get\",\"data\":{\"result\":0,\"output\":{\"file\":{\"url\":\"stable-url\",\"fingerprint\":\"stable-sign\"}}}}");
	TestTrue(TEXT("航线资源基线成功"), Bridge->DispatchRequestsReplyMessage(GoodWayline));
	Bridge->BuildTrackedRequestMessage(kRequestFlighttaskResourceGet,
		Bridge->BuildFlighttaskResourceGetRequestData(TEXT("FLT-BAD")), TEXT("tid-bad-wayline"), TEXT("bid-bad-wayline"), TEXT("FLT-BAD"));
	const FString BadWayline = TEXT("{\"tid\":\"tid-bad-wayline\",\"bid\":\"bid-bad-wayline\",\"method\":\"flighttask_resource_get\",\"data\":{\"result\":0,\"output\":{\"file\":{\"url\":\"bad-url\"}}}}");
	TestFalse(TEXT("缺 fingerprint 失败"), Bridge->DispatchRequestsReplyMessage(BadWayline));
	TestEqual(TEXT("失败航线保留旧 flight_id"), TEXT("FLT-OLD"), Bridge->GetFlighttaskResourceState().FlightId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVResourceSyncTimeoutTest, "UAV.MqttBridge.ResourceRequests.SyncTimeout", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVResourceSyncTimeoutTest::RunTest(const FString& Parameters)
{
	using namespace UAV::CloudApi;

	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	FUAVResourceFileState File;
	File.Name = TEXT("geofence_0123456789abcdef0123456789abcdef.json");
	File.Checksum = TEXT("sync-sha256");
	const TSharedPtr<FJsonObject> Success = Bridge->BuildResourceSyncProgressEventData(TEXT("synchronized"), 0, { File });
	TestEqual(TEXT("同步成功 status"), TEXT("synchronized"), Success->GetStringField(TEXT("status")));
	TestEqual(TEXT("同步成功 reason"), 0.0, Success->GetNumberField(TEXT("reason")));
	TestEqual(TEXT("同步成功 file.name"), File.Name, Success->GetObjectField(TEXT("file"))->GetStringField(TEXT("name")));
	TestEqual(TEXT("同步成功 file.checksum"), File.Checksum, Success->GetObjectField(TEXT("file"))->GetStringField(TEXT("checksum")));

	const TSharedPtr<FJsonObject> Failed = Bridge->BuildResourceSyncProgressEventData(TEXT("fail"), 1, {});
	TestEqual(TEXT("同步失败 status"), TEXT("fail"), Failed->GetStringField(TEXT("status")));
	TestEqual(TEXT("同步失败 reason"), 1.0, Failed->GetNumberField(TEXT("reason")));
	TestFalse(TEXT("空文件失败事件不含 file"), Failed->HasField(TEXT("file")));

	Bridge->RequestTimeoutSeconds = 10.0f;
	Bridge->BuildTrackedRequestMessage(kRequestFlightAreasGet, Bridge->BuildEmptyResourceRequestData(),
		TEXT("tid-timeout"), TEXT("bid-timeout"), FString(), true, 100.0);
	TestEqual(TEXT("阈值前不超时"), 0, Bridge->ExpireTimedOutRequests(109.0));
	TestTrue(TEXT("阈值前仍在 pending"), Bridge->HasPendingRequest(TEXT("bid-timeout")));
	TestEqual(TEXT("达到阈值清理一个"), 1, Bridge->ExpireTimedOutRequests(110.0));
	TestFalse(TEXT("超时 pending 已移除"), Bridge->HasPendingRequest(TEXT("bid-timeout")));
	TestEqual(TEXT("重复清理不重复广播"), 0, Bridge->ExpireTimedOutRequests(120.0));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
