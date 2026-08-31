## 1. 协议常量

- [x] 1.1 UAVCore：UAVCloudApiTypes.h/.cpp 新增 cloud_control_auth_request / cloud_control_release / fileupload_start / fileupload_update / fileupload_list / upload_flighttask_media_prioritize 六个 services method 常量
- [x] 1.2 UAVCore：UAVCloudApiTypes.h/.cpp 新增 fileupload_progress / cloud_control_auth_notify / highest_priority_upload_flighttask_media 三个事件常量

## 2. 指令处理与回执

- [x] 2.1 UAVMqttBridge：新增 HandleCloudControlAuthRequest / HandleCloudControlRelease（校验 user_id / user_callsign / control_keys 非空，control_keys 仅 flight/payload）
- [x] 2.2 UAVMqttBridge：新增 HandleFileUploadStart / HandleFileUploadUpdate（校验必填字段与枚举，fileupload_update status 仅 cancel、moduleList 1-2 项）
- [x] 2.3 UAVMqttBridge：新增 HandleFileUploadList（校验 moduleList 并生成合成文件列表）与 HandleMediaPrioritize（校验 flight_id 格式）
- [x] 2.4 UAVMqttBridge：DispatchServicesMessage 精确分发六类指令，成功后回发 services_reply（fileupload_list 带 output.files，其余带 output.status="sent"）

## 3. 事件模拟

- [x] 3.1 UAVMqttBridge：新增 BuildCloudControlAuthNotifyData（data:{result:0, output:{status:"ok", result:0}}）并在授权请求成功后发布
- [x] 3.2 UAVMqttBridge：新增 BuildFileUploadProgressEventData（data:{result:0, output:{status, ext:{files}}}）并在 fileupload_start 成功后发布 sent → ok 两段
- [x] 3.3 UAVMqttBridge：新增 BuildMediaPrioritizeEventData（data:{flightId}）并在媒体优先上传成功后发布

## 4. 自动化测试

- [x] 4.1 UAVMqttBridge/Tests：新增 UAVControlUploadTests.cpp 断言授权/释放指令校验（字段缺失、control_keys 非法）
- [x] 4.2 UAVMqttBridge/Tests：断言 fileupload_start/update/list 校验（必填字段、枚举越界）与 media_prioritize flight_id 格式
- [x] 4.3 UAVMqttBridge/Tests：断言回执结构（output.status="sent" / output.files）、授权事件、上传进度事件 sent→ok 与媒体优先事件结构

## 5. 构建与验证

- [x] 5.1 UBT 构建 UAVsimulatorEditor（Win64 Development）
- [x] 5.2 Automation RunTests UAV 全部通过（含新增用例与既有基线）
- [x] 5.3 openspec validate --specs 通过，按流程归档变更并 git commit + push
