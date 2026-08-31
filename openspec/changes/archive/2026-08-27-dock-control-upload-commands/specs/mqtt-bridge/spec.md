## ADDED Requirements

### Requirement: 云端控制权授权与释放指令
UAVMqttBridge MUST 将 services 通道收到的 cloud_control_auth_request 与 cloud_control_release 指令精确分发并处理（对齐 dock ControlMethodEnum.CLOUD_CONTROL_AUTH_REQUEST / CLOUD_CONTROL_RELEASE）。cloud_control_auth_request 的 data MUST 含非空 user_id / user_callsign / control_keys（control_keys 仅支持 "flight" 与 "payload"），缺失或非法时 MUST 返回 InvalidParams；校验通过后 MUST 回发 services_reply（data 为 { result: 0, output: { status: "sent" } }）并发布 cloud_control_auth_notify 事件（对齐 EventsMethodEnum.CLOUD_CONTROL_AUTH_NOTIFY / EventsDataRequest<CloudControlAuthNotify>，data 为 { result: 0, output: { status: "ok", result: 0 } }，模拟飞手同意授权）。cloud_control_release 的 data MUST 含非空 control_keys，校验通过后 MUST 回发 { result: 0, output: { status: "sent" } }。

#### Scenario: RC 链路授权请求
- **WHEN** 收到 method 为 cloud_control_auth_request 的 services 报文，data 含 user_id="cloud_user"、user_callsign="云端用户"、control_keys=["flight"]
- **THEN** 指令执行成功并回发 services_reply（result=0、output.status="sent"），随后发布 cloud_control_auth_notify 事件（status="ok"、result=0）

#### Scenario: 授权请求参数非法
- **WHEN** 收到 cloud_control_auth_request 且 user_id / user_callsign 缺失或 control_keys 为空/含非法键
- **THEN** 回发 result=InvalidParams 的 services_reply，不发布授权事件

### Requirement: 日志文件上传指令
UAVMqttBridge MUST 将 services 通道收到的 fileupload_start / fileupload_update / fileupload_list 指令精确分发并处理（对齐 dock LogMethodEnum）。fileupload_start 的 data MUST 含 bucket / credentials / endpoint / fileStoreDir / provider / region / params.files（1-2 个文件，文件含 deviceSn / list / module / objectKey），缺失或类型非法时 MUST 返回 InvalidParams；校验通过后 MUST 回发 { result: 0, output: { status: "sent" } } 并发布 fileupload_progress 事件（对齐 EventsMethodEnum.FILE_UPLOAD_PROGRESS / EventsDataRequest<FileUploadProgress>，data 为 { result: 0, output: { status, ext: { files: [FileUploadProgressFile] } } }），事件序列 MUST 依次为 status="sent"（progress.currentStep=1、totalStep=2、progress=0）与 status="ok"（progress=100）。fileupload_update 的 data MUST 含 moduleList（1-2 项）与 status="cancel"，校验通过后回发 output.status="sent"。fileupload_list 的 data MUST 含 moduleList，校验通过后回发 output.files（对齐 FileUploadListResponse，每项含 deviceSn / list / module / result）。

#### Scenario: 日志上传启动与进度
- **WHEN** 收到 fileupload_start 且 data 字段齐全合法
- **THEN** 回发 services_reply（result=0、output.status="sent"），随后依次发布 fileupload_progress 事件 sent（percent=0）与 ok（percent=100）

#### Scenario: 日志上传参数非法
- **WHEN** 收到 fileupload_start 且 bucket / credentials / params.files 缺失，或 fileupload_update 的 status 非 "cancel"、moduleList 数量越界
- **THEN** 回发 result=InvalidParams 的 services_reply，不发布上传进度事件

### Requirement: 媒体上传优先级指令
UAVMqttBridge MUST 将 services 通道收到的 upload_flighttask_media_prioritize 指令精确分发并处理（对齐 MediaMethodEnum.UPLOAD_FLIGHTTASK_MEDIA_PRIORITIZE）。data MUST 含非空 flight_id 且符合格式约束（不含 <>:"/|?*._\ 等字符），非法时 MUST 返回 InvalidParams；校验通过后 MUST 回发 { result: 0, output: { status: "sent" } } 并发布 highest_priority_upload_flighttask_media 事件（对齐 EventsMethodEnum.HIGHEST_PRIORITY_UPLOAD_FLIGHT_TASK_MEDIA / HighestPriorityUploadFlightTaskMedia，data 为 { flightId }）。

#### Scenario: 媒体上传优先级设置
- **WHEN** 收到 upload_flighttask_media_prioritize 且 data.flight_id 合法
- **THEN** 回发 services_reply（result=0、output.status="sent"），并发布 highest_priority_upload_flighttask_media 事件（data.flightId 与指令一致）

#### Scenario: 媒体优先级参数非法
- **WHEN** 收到 upload_flighttask_media_prioritize 且 flight_id 缺失或含非法字符
- **THEN** 回发 result=InvalidParams 的 services_reply，不发布媒体优先事件
