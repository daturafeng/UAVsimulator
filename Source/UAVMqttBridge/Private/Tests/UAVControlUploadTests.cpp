// 云端控制权授权/释放、日志上传、媒体优先上传自动化测试：指令参数校验 + 事件结构
#include "Misc/AutomationTest.h"
#include "UAVMqttBridgeComponent.h"
#include "UAVCloudApiTypes.h"
#include "UAVFlightControlComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** 合法日志文件项（module="0"=无人机，索引字段均为数字，对齐 FileUploadStartFile） */
	const FString kValidUploadFileJson = TEXT("{\"deviceSn\":\"1581F8HGXTEST001\",\"module\":\"0\",\"objectKey\":\"2026/08/27/log.zip\",\"list\":[{\"bootIndex\":1,\"startTime\":1000,\"endTime\":2000,\"size\":1024}]}");

	/** 合法 params.files 片段（供 fileupload_start 组合与替换断言使用） */
	const FString kValidUploadParamsJson = FString::Printf(TEXT("{\"files\":[%s]}"), *kValidUploadFileJson);

	/** 越界三文件 params.files 片段（对齐 FileUploadStartParams @Size(max=2)） */
	const FString kThreeFilesParamsJson = FString::Printf(TEXT("{\"files\":[%s,%s,%s]}"), *kValidUploadFileJson, *kValidUploadFileJson, *kValidUploadFileJson);

	/** 合法 fileupload_start data（对齐 FileUploadStartRequest 全部必填字段） */
	const FString kValidUploadStartJson = FString::Printf(TEXT("{\"bucket\":\"logs\",\"credentials\":{\"accessKeyId\":\"ak\",\"secretAccessKey\":\"sk\",\"sessionToken\":\"token\",\"expire\":3600},\"endpoint\":\"oss.example.com\",\"fileStoreDir\":\"/logs\",\"provider\":\"aliyun\",\"params\":{\"files\":[%s]},\"region\":\"cn-shanghai\"}"), *kValidUploadFileJson);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVControlAuthValidationTest, "UAV.MqttBridge.ControlUpload.AuthValidation", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVControlAuthValidationTest::RunTest(const FString& Parameters)
{
	using namespace UAV::FlightControlResult;

	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	// cloud_control_auth_request：合法授权成功（user_id / user_callsign / control_keys=flight,payload）
	TestEqual(TEXT("授权请求成功"), Success, Bridge->HandleCloudControlAuthRequest(TEXT("cloud_control_auth_request"), TEXT("{\"user_id\":\"user-001\",\"user_callsign\":\"flyer\",\"control_keys\":[\"flight\",\"payload\"]}")));
	// 未知方法
	TestEqual(TEXT("未知方法失败"), UnknownMethod, Bridge->HandleCloudControlAuthRequest(TEXT("cloud_control_release"), TEXT("{}")));
	// 字段缺失/非法
	TestEqual(TEXT("缺 user_id 失败"), InvalidParams, Bridge->HandleCloudControlAuthRequest(TEXT("cloud_control_auth_request"), TEXT("{\"user_callsign\":\"flyer\",\"control_keys\":[\"flight\"]}")));
	TestEqual(TEXT("缺 user_callsign 失败"), InvalidParams, Bridge->HandleCloudControlAuthRequest(TEXT("cloud_control_auth_request"), TEXT("{\"user_id\":\"user-001\",\"control_keys\":[\"flight\"]}")));
	TestEqual(TEXT("缺 control_keys 失败"), InvalidParams, Bridge->HandleCloudControlAuthRequest(TEXT("cloud_control_auth_request"), TEXT("{\"user_id\":\"user-001\",\"user_callsign\":\"flyer\"}")));
	TestEqual(TEXT("control_keys 为空失败"), InvalidParams, Bridge->HandleCloudControlAuthRequest(TEXT("cloud_control_auth_request"), TEXT("{\"user_id\":\"user-001\",\"user_callsign\":\"flyer\",\"control_keys\":[]}")));
	TestEqual(TEXT("control_keys 含 fpv 失败"), InvalidParams, Bridge->HandleCloudControlAuthRequest(TEXT("cloud_control_auth_request"), TEXT("{\"user_id\":\"user-001\",\"user_callsign\":\"flyer\",\"control_keys\":[\"flight\",\"fpv\"]}")));
	TestEqual(TEXT("非法 JSON 失败"), InvalidParams, Bridge->HandleCloudControlAuthRequest(TEXT("cloud_control_auth_request"), TEXT("{broken")));

	// cloud_control_release：合法释放成功，仅校验 control_keys
	TestEqual(TEXT("释放成功"), Success, Bridge->HandleCloudControlRelease(TEXT("cloud_control_release"), TEXT("{\"control_keys\":[\"flight\"]}")));
	TestEqual(TEXT("释放未知方法失败"), UnknownMethod, Bridge->HandleCloudControlRelease(TEXT("cloud_control_auth_request"), TEXT("{\"control_keys\":[\"flight\"]}")));
	TestEqual(TEXT("释放缺 control_keys 失败"), InvalidParams, Bridge->HandleCloudControlRelease(TEXT("cloud_control_release"), TEXT("{}")));
	TestEqual(TEXT("释放 control_keys 为空失败"), InvalidParams, Bridge->HandleCloudControlRelease(TEXT("cloud_control_release"), TEXT("{\"control_keys\":[]}")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVFileUploadValidationTest, "UAV.MqttBridge.ControlUpload.FileUploadValidation", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVFileUploadValidationTest::RunTest(const FString& Parameters)
{
	using namespace UAV::FlightControlResult;

	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	// fileupload_start：合法参数成功
	TestEqual(TEXT("上传启动成功"), Success, Bridge->HandleFileUploadStart(TEXT("fileupload_start"), kValidUploadStartJson));
	// 未知方法
	TestEqual(TEXT("未知方法失败"), UnknownMethod, Bridge->HandleFileUploadStart(TEXT("fileupload_update"), kValidUploadStartJson));
	// 必填字段缺失
	TestEqual(TEXT("缺 bucket 失败"), InvalidParams, Bridge->HandleFileUploadStart(TEXT("fileupload_start"), kValidUploadStartJson.Replace(TEXT("\"bucket\":\"logs\","), TEXT(""), ESearchCase::CaseSensitive)));
	TestEqual(TEXT("缺 endpoint 失败"), InvalidParams, Bridge->HandleFileUploadStart(TEXT("fileupload_start"), kValidUploadStartJson.Replace(TEXT("\"endpoint\":\"oss.example.com\","), TEXT(""), ESearchCase::CaseSensitive)));
	TestEqual(TEXT("缺 fileStoreDir 失败"), InvalidParams, Bridge->HandleFileUploadStart(TEXT("fileupload_start"), kValidUploadStartJson.Replace(TEXT("\"fileStoreDir\":\"/logs\","), TEXT(""), ESearchCase::CaseSensitive)));
	TestEqual(TEXT("缺 provider 失败"), InvalidParams, Bridge->HandleFileUploadStart(TEXT("fileupload_start"), kValidUploadStartJson.Replace(TEXT("\"provider\":\"aliyun\","), TEXT(""), ESearchCase::CaseSensitive)));
	TestEqual(TEXT("缺 region 失败"), InvalidParams, Bridge->HandleFileUploadStart(TEXT("fileupload_start"), kValidUploadStartJson.Replace(TEXT(",\"region\":\"cn-shanghai\""), TEXT(""), ESearchCase::CaseSensitive)));
	TestEqual(TEXT("缺 credentials 失败"), InvalidParams, Bridge->HandleFileUploadStart(TEXT("fileupload_start"), kValidUploadStartJson.Replace(TEXT("\"credentials\":{\"accessKeyId\":\"ak\",\"secretAccessKey\":\"sk\",\"sessionToken\":\"token\",\"expire\":3600},"), TEXT(""), ESearchCase::CaseSensitive)));
	TestEqual(TEXT("credentials 为空对象失败"), InvalidParams, Bridge->HandleFileUploadStart(TEXT("fileupload_start"), kValidUploadStartJson.Replace(TEXT("\"credentials\":{\"accessKeyId\":\"ak\",\"secretAccessKey\":\"sk\",\"sessionToken\":\"token\",\"expire\":3600}"), TEXT("\"credentials\":{}"), ESearchCase::CaseSensitive)));
	TestEqual(TEXT("缺 params 失败"), InvalidParams, Bridge->HandleFileUploadStart(TEXT("fileupload_start"), kValidUploadStartJson.Replace(*(TEXT(",\"params\":") + kValidUploadParamsJson), TEXT(""), ESearchCase::CaseSensitive)));
	// params.files 为空 / 越界 / 文件项非法
	TestEqual(TEXT("files 为空失败"), InvalidParams, Bridge->HandleFileUploadStart(TEXT("fileupload_start"), kValidUploadStartJson.Replace(*kValidUploadParamsJson, TEXT("{\"files\":[]}"), ESearchCase::CaseSensitive)));
	TestEqual(TEXT("files 三个失败"), InvalidParams, Bridge->HandleFileUploadStart(TEXT("fileupload_start"), kValidUploadStartJson.Replace(*kValidUploadParamsJson, *kThreeFilesParamsJson, ESearchCase::CaseSensitive)));
	TestEqual(TEXT("文件项缺 objectKey 失败"), InvalidParams, Bridge->HandleFileUploadStart(TEXT("fileupload_start"), kValidUploadStartJson.Replace(TEXT("\"objectKey\":\"2026/08/27/log.zip\","), TEXT(""), ESearchCase::CaseSensitive)));
	TestEqual(TEXT("文件项 module 非法失败"), InvalidParams, Bridge->HandleFileUploadStart(TEXT("fileupload_start"), kValidUploadStartJson.Replace(TEXT("\"module\":\"0\""), TEXT("\"module\":\"2\""), ESearchCase::CaseSensitive)));
	TestEqual(TEXT("文件项索引非数字失败"), InvalidParams, Bridge->HandleFileUploadStart(TEXT("fileupload_start"), kValidUploadStartJson.Replace(TEXT("\"size\":1024"), TEXT("\"size\":\"big\""), ESearchCase::CaseSensitive)));
	TestEqual(TEXT("空 data 失败"), InvalidParams, Bridge->HandleFileUploadStart(TEXT("fileupload_start"), TEXT("")));

	// fileupload_update：status=cancel 且 moduleList 1-2 项
	TestEqual(TEXT("更新取消成功"), Success, Bridge->HandleFileUploadUpdate(TEXT("fileupload_update"), TEXT("{\"moduleList\":[\"0\",\"3\"],\"status\":\"cancel\"}")));
	TestEqual(TEXT("更新未知方法失败"), UnknownMethod, Bridge->HandleFileUploadUpdate(TEXT("fileupload_start"), TEXT("{\"moduleList\":[\"0\"],\"status\":\"cancel\"}")));
	TestEqual(TEXT("更新 status 非 cancel 失败"), InvalidParams, Bridge->HandleFileUploadUpdate(TEXT("fileupload_update"), TEXT("{\"moduleList\":[\"0\"],\"status\":\"failed\"}")));
	TestEqual(TEXT("更新 moduleList 为空失败"), InvalidParams, Bridge->HandleFileUploadUpdate(TEXT("fileupload_update"), TEXT("{\"moduleList\":[],\"status\":\"cancel\"}")));
	TestEqual(TEXT("更新 moduleList 三项失败"), InvalidParams, Bridge->HandleFileUploadUpdate(TEXT("fileupload_update"), TEXT("{\"moduleList\":[\"0\",\"3\",\"0\"],\"status\":\"cancel\"}")));
	TestEqual(TEXT("更新 moduleList 非法失败"), InvalidParams, Bridge->HandleFileUploadUpdate(TEXT("fileupload_update"), TEXT("{\"moduleList\":[\"5\"],\"status\":\"cancel\"}")));

	// fileupload_list：moduleList 合法即成功
	TestEqual(TEXT("列表查询成功"), Success, Bridge->HandleFileUploadList(TEXT("fileupload_list"), TEXT("{\"moduleList\":[\"0\",\"3\"]}")));
	TestEqual(TEXT("列表未知方法失败"), UnknownMethod, Bridge->HandleFileUploadList(TEXT("fileupload_start"), TEXT("{\"moduleList\":[\"0\"]}")));
	TestEqual(TEXT("列表 moduleList 为空失败"), InvalidParams, Bridge->HandleFileUploadList(TEXT("fileupload_list"), TEXT("{\"moduleList\":[]}")));
	TestEqual(TEXT("列表 moduleList 非法失败"), InvalidParams, Bridge->HandleFileUploadList(TEXT("fileupload_list"), TEXT("{\"moduleList\":[\"9\"]}")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVMediaPrioritizeValidationTest, "UAV.MqttBridge.ControlUpload.MediaPrioritizeValidation", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVMediaPrioritizeValidationTest::RunTest(const FString& Parameters)
{
	using namespace UAV::FlightControlResult;

	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	// upload_flighttask_media_prioritize：合法 flight_id 成功
	TestEqual(TEXT("媒体优先成功"), Success, Bridge->HandleMediaPrioritize(TEXT("upload_flighttask_media_prioritize"), TEXT("{\"flight_id\":\"20260827120000\"}")));
	TestEqual(TEXT("未知方法失败"), UnknownMethod, Bridge->HandleMediaPrioritize(TEXT("fileupload_start"), TEXT("{\"flight_id\":\"20260827120000\"}")));
	// flight_id 缺失 / 空 / 含非法字符（对齐 @Pattern 排除集）
	TestEqual(TEXT("缺 flight_id 失败"), InvalidParams, Bridge->HandleMediaPrioritize(TEXT("upload_flighttask_media_prioritize"), TEXT("{}")));
	TestEqual(TEXT("flight_id 为空失败"), InvalidParams, Bridge->HandleMediaPrioritize(TEXT("upload_flighttask_media_prioritize"), TEXT("{\"flight_id\":\"\"}")));
	TestEqual(TEXT("flight_id 含斜杠失败"), InvalidParams, Bridge->HandleMediaPrioritize(TEXT("upload_flighttask_media_prioritize"), TEXT("{\"flight_id\":\"2026/08/27\"}")));
	TestEqual(TEXT("flight_id 含点失败"), InvalidParams, Bridge->HandleMediaPrioritize(TEXT("upload_flighttask_media_prioritize"), TEXT("{\"flight_id\":\"2026.0827\"}")));
	TestEqual(TEXT("flight_id 含下划线失败"), InvalidParams, Bridge->HandleMediaPrioritize(TEXT("upload_flighttask_media_prioritize"), TEXT("{\"flight_id\":\"20260827_1200\"}")));
	TestEqual(TEXT("非法 JSON 失败"), InvalidParams, Bridge->HandleMediaPrioritize(TEXT("upload_flighttask_media_prioritize"), TEXT("{broken")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUAVControlUploadEventTest, "UAV.MqttBridge.ControlUpload.EventStructure", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUAVControlUploadEventTest::RunTest(const FString& Parameters)
{
	UUAVMqttBridgeComponent* Bridge = NewObject<UUAVMqttBridgeComponent>();
	TestNotNull(TEXT("桥接组件可创建"), Bridge);
	if (!Bridge)
	{
		return false;
	}

	// cloud_control_auth_notify：data={result:0, output:{status, result}}（对齐 EventsDataRequest<CloudControlAuthNotify>）
	const TSharedPtr<FJsonObject> AuthNotify = Bridge->BuildCloudControlAuthNotifyData(TEXT("ok"), 0);
	TestTrue(TEXT("授权事件 data 已组装"), AuthNotify.IsValid());
	if (AuthNotify.IsValid())
	{
		TestEqual(TEXT("授权事件 result=0"), 0.0, AuthNotify->GetNumberField(TEXT("result")));
		const TSharedPtr<FJsonObject> Output = AuthNotify->GetObjectField(TEXT("output"));
		TestTrue(TEXT("授权事件 output 对象存在"), Output.IsValid());
		if (Output.IsValid())
		{
			TestEqual(TEXT("授权事件 output.status=ok"), TEXT("ok"), Output->GetStringField(TEXT("status")));
			TestEqual(TEXT("授权事件 output.result=0"), 0.0, Output->GetNumberField(TEXT("result")));
		}
	}

	// fileupload_progress sent：progress 携带 currentStep=1/totalStep=2/progress=0/uploadRate=0
	const TSharedPtr<FJsonObject> Sent = Bridge->BuildFileUploadProgressEventData(TEXT("sent"), 0, TEXT("3"), TEXT("DOCK3TEST001"));
	TestTrue(TEXT("上传进度 sent data 已组装"), Sent.IsValid());
	if (Sent.IsValid())
	{
		TestEqual(TEXT("sent result=0"), 0.0, Sent->GetNumberField(TEXT("result")));
		const TSharedPtr<FJsonObject> Output = Sent->GetObjectField(TEXT("output"));
		TestTrue(TEXT("sent output 对象存在"), Output.IsValid());
		if (Output.IsValid())
		{
			TestEqual(TEXT("sent output.status"), TEXT("sent"), Output->GetStringField(TEXT("status")));
			const TArray<TSharedPtr<FJsonValue>>* Files = nullptr;
			TestTrue(TEXT("sent ext.files 数组存在"), Output->GetObjectField(TEXT("ext"))->TryGetArrayField(TEXT("files"), Files));
			if (Files && Files->Num() == 1)
			{
				const TSharedPtr<FJsonObject> File = (*Files)[0]->AsObject();
				TestEqual(TEXT("sent file.module"), TEXT("3"), File->GetStringField(TEXT("module")));
				TestEqual(TEXT("sent file.deviceSn"), TEXT("DOCK3TEST001"), File->GetStringField(TEXT("deviceSn")));
				TestTrue(TEXT("sent file.key 非空"), !File->GetStringField(TEXT("key")).IsEmpty());
				const TSharedPtr<FJsonObject> Progress = File->GetObjectField(TEXT("progress"));
				TestTrue(TEXT("sent progress 对象存在"), Progress.IsValid());
				if (Progress.IsValid())
				{
					TestEqual(TEXT("sent currentStep=1"), 1.0, Progress->GetNumberField(TEXT("currentStep")));
					TestEqual(TEXT("sent totalStep=2"), 2.0, Progress->GetNumberField(TEXT("totalStep")));
					TestEqual(TEXT("sent progress=0"), 0.0, Progress->GetNumberField(TEXT("progress")));
					TestEqual(TEXT("sent uploadRate=0"), 0.0, Progress->GetNumberField(TEXT("uploadRate")));
					TestEqual(TEXT("sent status 同步"), TEXT("sent"), Progress->GetStringField(TEXT("status")));
					TestEqual(TEXT("sent result=0"), 0.0, Progress->GetNumberField(TEXT("result")));
				}
			}
			else
			{
				TestTrue(TEXT("sent files 数量为 1"), false);
			}
		}
	}

	// fileupload_progress ok：percent=100、currentStep=2
	const TSharedPtr<FJsonObject> Ok = Bridge->BuildFileUploadProgressEventData(TEXT("ok"), 100, TEXT("0"), TEXT("1581F8HGXTEST001"));
	TestEqual(TEXT("ok output.status"), TEXT("ok"), Ok->GetObjectField(TEXT("output"))->GetStringField(TEXT("status")));
	const TArray<TSharedPtr<FJsonValue>>* OkFiles = nullptr;
	Ok->GetObjectField(TEXT("output"))->GetObjectField(TEXT("ext"))->TryGetArrayField(TEXT("files"), OkFiles);
	TestTrue(TEXT("ok files 数组存在"), OkFiles && OkFiles->Num() == 1);
	if (OkFiles && OkFiles->Num() == 1)
	{
		const TSharedPtr<FJsonObject> Progress = (*OkFiles)[0]->AsObject()->GetObjectField(TEXT("progress"));
		TestEqual(TEXT("ok currentStep=2"), 2.0, Progress->GetNumberField(TEXT("currentStep")));
		TestEqual(TEXT("ok progress=100"), 100.0, Progress->GetNumberField(TEXT("progress")));
		TestEqual(TEXT("ok status 同步"), TEXT("ok"), Progress->GetStringField(TEXT("status")));
	}

	// highest_priority_upload_flighttask_media：data={flightId}
	const TSharedPtr<FJsonObject> Media = Bridge->BuildMediaPrioritizeEventData(TEXT("20260827120000"));
	TestEqual(TEXT("媒体优先事件 flightId"), TEXT("20260827120000"), Media->GetStringField(TEXT("flightId")));

	// fileupload_list 回执 output：files 两条（机场 module=3 / 无人机 module=0），每项含 deviceSn/list/module/result
	const TSharedPtr<FJsonObject> ListOutput = Bridge->BuildFileUploadListOutput();
	TestTrue(TEXT("列表回执 output 已组装"), ListOutput.IsValid());
	const TArray<TSharedPtr<FJsonValue>>* ListFiles = nullptr;
	TestTrue(TEXT("列表 files 数组存在"), ListOutput->TryGetArrayField(TEXT("files"), ListFiles));
	if (ListFiles)
	{
		TestEqual(TEXT("列表 files 数量为 2"), 2, ListFiles->Num());
		if (ListFiles->Num() == 2)
		{
			const TSharedPtr<FJsonObject> DockFile = (*ListFiles)[0]->AsObject();
			TestEqual(TEXT("机场 deviceSn"), TEXT("DOCK3TEST001"), DockFile->GetStringField(TEXT("deviceSn")));
			TestEqual(TEXT("机场 module=3"), TEXT("3"), DockFile->GetStringField(TEXT("module")));
			TestEqual(TEXT("机场 result=0"), 0.0, DockFile->GetNumberField(TEXT("result")));
			const TArray<TSharedPtr<FJsonValue>>* DockList = nullptr;
			TestTrue(TEXT("机场 list 数组存在"), DockFile->TryGetArrayField(TEXT("list"), DockList));
			if (DockList && DockList->Num() == 1)
			{
				TestEqual(TEXT("机场索引 bootIndex=1"), 1.0, (*DockList)[0]->AsObject()->GetNumberField(TEXT("bootIndex")));
			}
			const TSharedPtr<FJsonObject> DroneFile = (*ListFiles)[1]->AsObject();
			TestEqual(TEXT("无人机 deviceSn"), TEXT("1581F8HGXTEST001"), DroneFile->GetStringField(TEXT("deviceSn")));
			TestEqual(TEXT("无人机 module=0"), TEXT("0"), DroneFile->GetStringField(TEXT("module")));
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
