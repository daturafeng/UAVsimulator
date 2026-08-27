# mqtt-bridge Delta Spec

## MODIFIED Requirements

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

### Requirement: 自动返航事件
UAVMqttBridge MUST 绑定 UAVFlightControl.OnReturnHomeStatus，将 rth_auto_trigger 事件拼装为 thing/product/{机场SN}/events 报文（method=return_home_info，对齐 dock EventsMethodEnum / ReturnHomeInfo）发布，data 含 planned_path_points（返航航线点数组，元素含 latitude/longitude/height）、last_point_type（有直飞返航点 0、有航线路径 1、无航点 65535）、flight_id；不再发布 return_home_status 报文。

#### Scenario: 自动返航事件转发
- **WHEN** 飞控广播 OnReturnHomeStatus（rth_auto_trigger/battery_low）
- **THEN** 发布 events 报文 return_home_info，data 含 planned_path_points / last_point_type / flight_id，且不发布 method 为 return_home_status 的报文

## ADDED Requirements

### Requirement: 任务就绪事件上报
UAVMqttBridge MUST 绑定 UAVFlightControl.OnFlighttaskReady：flighttask_prepare 成功后飞控广播任务就绪，桥接层发布 thing/product/{机场SN}/events 报文（method=flighttask_ready），data = { flight_ids: [flight_id] }，对齐 dock FlightTaskServiceImpl.flighttaskReady。

#### Scenario: 任务就绪事件
- **WHEN** 飞控广播 OnFlighttaskReady（携带 flight_id）
- **THEN** 发布 events 报文 flighttask_ready，data.flight_ids 数组含该 flight_id

### Requirement: HMS 告警事件上报
UAVMqttBridge MUST 在 MQTT 连接成功后发布 thing/product/{机场SN}/events 报文（method=hms）空告警 data = { list: [] }；低电量自动返航触发时（OnReturnHomeStatus 为 rth_auto_trigger/battery_low）发布 hms 告警 data = { list: [{ code, device_type, imminent, in_the_sky, level, module, args: { component_index } }] }，字段名与类型对齐 dock Hms / DeviceHms / DeviceHmsArgs（Jackson snake_case）。

#### Scenario: 连接成功后空告警
- **WHEN** MQTT 连接成功
- **THEN** 发布 hms 事件，data.list 为空数组

#### Scenario: 低电量告警
- **WHEN** 低电量自动返航触发（rth_auto_trigger/battery_low）
- **THEN** 发布 hms 事件，data.list 含一条告警：code=fpv_tip_0x1B030014、device_type=0-100-1、imminent=true、in_the_sky=true、level=1、module=0、args.component_index=0
