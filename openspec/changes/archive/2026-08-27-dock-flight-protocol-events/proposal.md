## Why

模拟器与 dock 的事件通道存在协议错位与缺口，导致 dock 端任务状态机与告警记录不完整：

- `flighttask_progress` 报文结构与 dock 不符：dock `SDKWaylineService.flighttaskProgress` 期望 `data = { result, output }`（`EventsDataRequest<FlighttaskProgress>`），`output = { status, progress, ext }`；当前模拟器发布的是平铺结构 `{ status, flight_id, currentWaypointIndex, percent, ext }`，dock 反序列化后 `getOutput()` 为 null，每次进度上报都报 `航线任务进度上报缺少 output`，任务状态机（setRunningWaylineJob / updateJob / WebSocket 推送）全部失效。
- `return_home_status` 不是 dock 支持的 method：dock `EventsMethodEnum` 只识别 `return_home_info`（`ReturnHomeInfo`：planned_path_points / last_point_type / flight_id），当前事件会落入 UNKNOWN 被丢弃。
- `flighttask_ready` 事件缺失：dock `FlightTaskServiceImpl.flighttaskReady` 消费 `data.flight_ids` 推进条件任务，模拟器 prepare 完成后未上报。
- `hms` 事件缺失：dock `DeviceHmsServiceImpl.hms` 消费 `data.list`（DeviceHms 数组）记录设备告警，模拟器连接后从未上报，dock HMS 记录为空。

## What Changes

- 修正 `flighttask_progress` 事件报文结构对齐 dock `EventsDataRequest<FlighttaskProgress>`：`data = { result: 0, output: { status, progress: { current_step, percent }, ext: { current_waypoint_index, media_count, flight_id, track_id, wayline_id, wayline_mission_state } } }`。**BREAKING**（报文结构变更，dock 端依赖 output 结构）。
- `return_home_status` 替换为 `return_home_info`：`data = { planned_path_points: [{ latitude, longitude, height }], last_point_type: 0|1, flight_id }`。**BREAKING**（method 与 data 结构变更）。
- 新增 `flighttask_ready` 事件：`flighttask_prepare` 成功后广播，`data = { flight_ids: [flight_id] }`。
- 新增 `hms` 事件上报：MQTT 连接成功后发空告警 `{ list: [] }`；低电量自动返航触发时发 `{ list: [{ code: "fpv_tip_0x1B030014", device_type: "0-100-1", imminent: true, in_the_sky: true, level: 1, module: 0, args: { component_index: 0 } }] }`，字段名对齐 dock Hms / DeviceHms / DeviceHmsArgs（Jackson snake_case）。

## Capabilities

### New Capabilities
<!-- 无：能力建立在已有 mqtt-bridge 与 flight-control-protocol 之上 -->

### Modified Capabilities
- mqtt-bridge: 事件转发对齐 dock——flighttask_progress 改为 result/output 包装结构、return_home_info 替换 return_home_status、新增 flighttask_ready 与 hms 上报。
- flight-control-protocol: 新增任务就绪事件触发点（flighttask_prepare 成功后广播 flighttask_ready）；返航状态事件语义对齐 ReturnHomeInfo（planned_path_points / last_point_type / flight_id）。

## Impact

- Source/UAVCore/Public/UAVCloudApiTypes.h/.cpp：事件 method 常量新增（`kEventReturnHomeInfo`、`kEventFlighttaskReady`、`kEventHms`），移除/保留 `kEventReturnHomeStatus`（不再发布）。
- Source/UAVFlightControl：新增 `OnFlighttaskReady` 动态委托（flight_id 参数），`HandleFlighttaskPrepare` 成功后广播；返航事件保持 OnReturnHomeStatus 委托（桥接层负责转换为 return_home_info）。
- Source/UAVMqttBridge：OnFlighttaskProgress 报文重构（result/output 包装 + progress/ext 字段）、OnReturnHomeStatus → return_home_info 报文、连接成功发布 hms 空告警、低电量自动返航时发布 hms 告警。
- Source/UAVMqttBridge/Private/Tests：新增事件报文结构断言（flighttask_progress / return_home_info / flighttask_ready / hms）。
- 行为变化：dock 端航线任务进度状态机恢复正常、返航信息可被记录、条件任务可被 ready 事件推进、HMS 告警列表可见。
