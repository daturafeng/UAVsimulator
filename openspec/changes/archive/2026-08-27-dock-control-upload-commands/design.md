## Context

桥接层（UUAVMqttBridgeComponent）已承担 services 分发（DispatchServicesMessage）、events 发布（PublishEvent）与 services_reply 回发（PublishServicesReply）。本变更三组指令均为纯状态模拟：授权由模拟器代飞手同意（自动 ok）、日志上传不产生真实文件、媒体优先上传仅记录 flight_id，状态托管在桥接组件私有成员即可，无需新增模块。协议以 dock cloud-sdk 的 ControlMethodEnum / LogMethodEnum / MediaMethodEnum / EventsMethodEnum 为口径（见 proposal.md - Why）。

## Goals / Non-Goals

**Goals:**
- services 通道精确识别 cloud_control_auth_request / cloud_control_release / fileupload_start / fileupload_update / fileupload_list / upload_flighttask_media_prioritize 六类指令并按 dock 参数模型校验。
- 成功后回发 services_reply（对齐 ServicesReplyData：result=0，fileupload_start/update 与 media_prioritize 带 output.status="sent"，fileupload_list 带 output.files 对齐 FileUploadListResponse）。
- 模拟异步回调：授权成功发布 cloud_control_auth_notify（status="ok", result=0）；日志上传发布 fileupload_progress（sent → ok 两段）；媒体优先上传发布 highest_priority_upload_flighttask_media。
- 新增自动化测试覆盖指令校验、回执结构、事件序列与列表结构。

**Non-Goals:**
- 不实现真实文件上传/下载、不接入对象存储、不产生日志文件实体。
- 不实现飞手拒绝/取消授权的交互（统一模拟同意），不联动飞控行为。
- 不实现 requests 通道（storage_config_get 等设备主动查询），后续变更另行覆盖。

## Decisions

1. **授权结果同步模拟**：cloud_control_auth_request 校验通过后回发 output.status="sent"，随后立即发布 cloud_control_auth_notify（status="ok"、result=0）事件，模拟飞手同意；不引入定时器，与远程调试进度事件同模式。
2. **上传进度事件两段发布**：fileupload_start 成功后立即发布 sent（files 含 progress.currentStep=1/totalStep=2/progress=0）与 ok（progress=100）两条事件，对齐 FileUploadStatusEnum；fileupload_update 仅校验并回执，不改变事件序列。
3. **fileupload_list 返回合成列表**：按 moduleList（0=无人机 / 3=机场）生成机场与无人机各一条文件记录（list 含 bootIndex/startTime/endTime/size），回执 output.files 对齐 FileUploadListResponse；纯模拟数据在设计与测试中显式记录。
4. **媒体优先上传回发事件**：upload_flighttask_media_prioritize 校验 flight_id（对齐 @Pattern）后回发 output.status="sent"，并发布 highest_priority_upload_flighttask_media 事件（data={flightId}，对齐 HighestPriorityUploadFlightTaskMedia）。
5. **精确方法匹配**：DispatchServicesMessage 对六个 method 新增精确分支（cloud_control_* / fileupload_* 无共用前缀可安全前缀匹配；media_prioritize 需精确匹配），命中后调用对应处理入口。

## Risks / Trade-offs

- 授权自动同意 → RC 链路仅演示成功路径；拒绝/取消语义明确记录在 Non-Goals，后续可按需扩展。
- 同步两段进度事件在极短时间窗内连续发布 → dock 端按 bid 关联事件，顺序由发布时序维持，与 OTA/远程调试进度事件同模式。
- 日志列表为合成数据 → 仅影响 dock-fe 日志页展示，不涉及真实设备文件；字段结构对齐 SDK 反序列化要求。
