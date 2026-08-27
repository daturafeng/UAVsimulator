# mqtt-bridge Delta Spec

## MODIFIED Requirements

### Requirement: 指令订阅与分发
UAVMqttBridge MUST 订阅 thing/product/{机场SN}/services，将收到的 services 报文（tid/bid/method/data）按 method 分发到对应处理组件（UAVFlightControl / UAVCameraStream），并把处理结果（result 码）回发到 thing/product/{机场SN}/services_reply。飞控指令分发 MUST 覆盖 fly_to_point / fly_to_point_stop / fly_to_point_update 前缀（fly_）。

#### Scenario: 服务指令分发
- **WHEN** 收到 method 为 takeoff_to_point / flighttask_* / return_home_* / flight_authority_grab 的 services 报文
- **THEN** 调用 UAVFlightControl.HandleCommand，并将返回的 result 码组装 services_reply 发布

#### Scenario: 直播指令分发
- **WHEN** 收到 method 为 live_start_push / live_stop_push / live_set_quality / live_lens_change 的 services 报文
- **THEN** 调用 UAVCameraStream.HandleCommand，并将返回的 result 码组装 services_reply 发布

#### Scenario: 未知指令回复
- **WHEN** 收到不认识的 method
- **THEN** 回发 result 非 0 的 services_reply，不抛异常

#### Scenario: 指点飞行指令分发
- **WHEN** 收到 method 为 fly_to_point / fly_to_point_stop / fly_to_point_update 的 services 报文
- **THEN** 调用 UAVFlightControl.HandleCommand，并将返回的 result 码组装 services_reply 发布

### Requirement: 事件转发
UAVMqttBridge MUST 订阅 UAVFlightControl 的进度/结果委托与 UAVCameraStream 的直播状态委托，将事件拼装为 thing/product/{机场SN}/events 报文（含 tid/bid/timestamp/gateway/method/data）发布。

#### Scenario: 起飞到点进度事件
- **WHEN** 飞控广播 takeoff_to_point_progress（task_ready/wayline_progress/wayline_ok/task_finish）
- **THEN** 发布 events 报文，data 含 result/status/flight_id/track_id/way_point_index/remaining_distance

#### Scenario: 航线任务进度事件
- **WHEN** 飞控广播 flighttask_progress（sent/in_progress/ok/paused/rejected/failed）
- **THEN** 发布 events 报文，data 为 { result: 0, output: { status, progress: { current_step, percent }, ext: { current_waypoint_index, media_count, flight_id, track_id, wayline_id, wayline_mission_state } } }，对齐 dock EventsDataRequest<FlighttaskProgress>，output 必填且不可为 null

#### Scenario: 直播状态事件
- **WHEN** 相机直播状态变化（开始/停止/清晰度/镜头切换）
- **THEN** 发布 events 报文（live_status 等），data 含 status/video_id/video_quality/video_type

#### Scenario: 指点飞行进度事件
- **WHEN** 飞控广播 fly_to_point_progress（wayline_progress / wayline_ok / wayline_cancel）
- **THEN** 发布 events 报文，method=fly_to_point_progress，data 为 { result, status, fly_to_id, way_point_index }，对齐 dock FlyToPointProgress / FlyToStatusEnum
