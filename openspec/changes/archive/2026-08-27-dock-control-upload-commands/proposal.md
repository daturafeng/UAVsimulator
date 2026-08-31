## Why

模拟器已覆盖 services 通道的飞控/相机/DRC/远程调试/OTA 指令，但云端向遥控器申请控制权（RC 链路）、日志文件上传与媒体上传优先级这三组指令仍为缺口：dock 的 ControlServiceImpl 在 RC 链路下会下发 cloud_control_auth_request / cloud_control_release 并等待 cloud_control_auth_notify 事件回调；DeviceLogsServiceImpl 会下发 fileupload_start / fileupload_update（及 fileupload_list 查询），并依赖 fileupload_progress 事件缓存日志上传进度推送前端；FlightTaskServiceImpl 在媒体最高优先级上传时会下发 upload_flighttask_media_prioritize 并等待 highest_priority_upload_flighttask_media 事件。当前模拟器对这些指令统一回 UnknownMethod，前端 RC 直控授权、日志上传、媒体优先上传链路无法走通。

## What Changes

- UAVCore：新增 cloud_control_auth_request / cloud_control_release / fileupload_start / fileupload_update / fileupload_list / upload_flighttask_media_prioritize 六个 services method 常量，以及 fileupload_progress / cloud_control_auth_notify / highest_priority_upload_flighttask_media 三个事件常量，对齐 dock ControlMethodEnum / LogMethodEnum / MediaMethodEnum / EventsMethodEnum。
- UAVMqttBridge：新增 HandleCloudControlAuthRequest（校验 user_id / user_callsign / control_keys 非空，模拟飞手同意）、HandleCloudControlRelease（校验 control_keys 非空）、HandleFileUploadStart（校验 bucket / credentials / endpoint / fileStoreDir / provider / region / params.files）、HandleFileUploadUpdate（校验 moduleList 1-2 项与 status=cancel）、HandleFileUploadList（校验 moduleList 并回查文件列表）、HandleMediaPrioritize（校验 flight_id 格式）；DispatchServicesMessage 精确分发，成功后回发 services_reply（data:{result, output:{status:"sent"}}，fileupload_list 回 output.files 对齐 FileUploadListResponse）。
- UAVMqttBridge：成功后发布事件——cloud_control_auth_notify（data:{result:0, output:{status:"ok", result:0}}，模拟飞手同意授权）、fileupload_progress（sent → ok 两段，对齐 EventsDataRequest<FileUploadProgress>）、highest_priority_upload_flighttask_media（data:{flightId}，对齐 HighestPriorityUploadFlightTaskMedia）。
- 自动化测试：新增 UAVControlUploadTests.cpp 覆盖指令校验、回执结构、事件序列与列表结构。

## Capabilities

### New Capabilities

### Modified Capabilities
- mqtt-bridge: 新增云端控制权授权/释放、日志文件上传（start/update/list）与媒体上传优先级指令处理及对应事件上报。

## Impact

- Source/UAVCore：UAVCloudApiTypes.h/.cpp 新增 6 个 services method 常量与 3 个事件常量。
- Source/UAVMqttBridge：UAVMqttBridgeComponent 新增 6 个指令处理入口与事件 data 构建入口，DispatchServicesMessage 分发分支扩展。
- Source/UAVMqttBridge/Private/Tests：新增 UAVControlUploadTests.cpp。
- 行为变化：dock RC 链路授权流程可完成（cloud_control_auth_request → cloud_control_auth_notify），日志上传可启动并上报进度，媒体上传优先级可设置并回发事件。
