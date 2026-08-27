# mqtt-bridge Specification

## Purpose
打通 UAVsimulator 与 dock（大疆上云 API）之间的 MQTT 通道：模拟器扮演"机场 + 无人机"角色，订阅 dock 下发的 services 指令并执行，回发 services_reply 与 events，周期上报 OSD 遥测与设备状态。首期设备：机场 DOCK3TEST001、无人机 1581F8HGXTEST001（M4TD，相机索引 52-0-0）。
## Requirements
### Requirement: 连接管理
UAVMqttBridge MUST 提供可配置的 MQTT 连接（broker 地址、端口、用户名、密码、机场 SN、无人机 SN），通过项目内置 MQTT 插件（UAVMQTT，模块 UAVMQTTCore，源自引擎 MQTTCore 源码）建立/断开连接，并暴露连接状态事件（已连接/断开/失败）。

#### Scenario: 默认配置对齐 dock
- **WHEN** 组件未覆盖任何配置
- **THEN** 默认 broker 为 10.100.51.15:1883、用户名 root、密码 unis@123、机场 SN DOCK3TEST001、无人机 SN 1581F8HGXTEST001、相机索引 52-0-0

#### Scenario: 连接失败可感知
- **WHEN** broker 不可达或鉴权失败
- **THEN** 组件保持未连接状态并通过连接状态事件广播失败原因

### Requirement: 指令订阅与分发
UAVMqttBridge MUST 订阅 thing/product/{机场SN}/services，将收到的 services 报文（tid/bid/method/data）按 method 分发到对应处理组件（UAVFlightControl / UAVCameraStream），并把处理结果（result 码）回发到 thing/product/{机场SN}/services_reply。

#### Scenario: 服务指令分发
- **WHEN** 收到 method 为 takeoff_to_point / flighttask_* / return_home_* / flight_authority_grab 的 services 报文
- **THEN** 调用 UAVFlightControl.HandleCommand，并将返回的 result 码组装 services_reply 发布

#### Scenario: 直播指令分发
- **WHEN** 收到 method 为 live_start_push / live_stop_push / live_set_quality / live_lens_change 的 services 报文
- **THEN** 调用 UAVCameraStream.HandleCommand，并将返回的 result 码组装 services_reply 发布

#### Scenario: 未知指令回复
- **WHEN** 收到不认识的 method
- **THEN** 回发 result 非 0 的 services_reply，不抛异常

### Requirement: 事件转发
UAVMqttBridge MUST 订阅 UAVFlightControl 的进度/结果委托与 UAVCameraStream 的直播状态委托，将事件拼装为 thing/product/{机场SN}/events 报文（含 tid/bid/timestamp/gateway/method/data）发布。

#### Scenario: 起飞到点进度事件
- **WHEN** 飞控广播 takeoff_to_point_progress（task_ready/wayline_progress/wayline_ok/task_finish）
- **THEN** 发布 events 报文，data 含 result/status/flight_id/track_id/way_point_index/remaining_distance

#### Scenario: 航线任务进度事件
- **WHEN** 飞控广播 flighttask_progress（sent/in_progress/ok/paused/rejected/failed）
- **THEN** 发布 events 报文，data 含 status/flight_id/currentWaypointIndex/percent

#### Scenario: 直播状态事件
- **WHEN** 相机直播状态变化（开始/停止/清晰度/镜头切换）
- **THEN** 发布 events 报文（live_status 等），data 含 status/video_id/video_quality/video_type

### Requirement: OSD 遥测上报
UAVMqttBridge MUST 周期（默认 1 秒，可配置）从 UAVDroneSim 读取无人机位置/高度/朝向/速度，组装 OSD 报文发布到 thing/product/{无人机SN}/osd，字段结构对齐 dock report_drone_osd.py（经纬度、高度、朝向、水平/垂直速度、模式编码、电量、载荷云台、摄像头状态等）。

#### Scenario: 周期上报无人机 OSD
- **WHEN** 组件已连接且无人机模拟组件已注入
- **THEN** 每 1 秒向 thing/product/1581F8HGXTEST001/osd 发布包含实时位置与状态的报文

#### Scenario: 未注入模拟组件
- **WHEN** 无人机模拟组件未注入
- **THEN** OSD 周期任务不启动或跳过发布，不产生崩溃

### Requirement: 设备状态与在线状态
UAVMqttBridge MUST 在连接后发布设备在线状态（sys/product/{机场SN}/status 与 thing/product/{sn}/state），并在断连/退出时发布离线状态。

#### Scenario: 上线广播
- **WHEN** MQTT 连接成功
- **THEN** 发布机场与无人机的在线状态报文

#### Scenario: 下线广播
- **WHEN** MQTT 连接断开或组件销毁
- **THEN** 发布离线状态报文（尽力而为）

### Requirement: OSD 电量字段
UAVMqttBridge MUST 组装 OSD battery 段：batteries 数组含两个电池单元（index/temperature/voltage，数值由 UAVDroneSim 电池单元推导提供），capacity_percent 为电量取整（int），landing_power / return_home_power 从 UAVDroneSim 阈值读取（默认 20/25），remain_flight_time 从 UAVDroneSim 剩余飞行时间读取。

#### Scenario: 电量上报随模拟状态变化
- **WHEN** UAVDroneSim 电量下降
- **THEN** OSD battery.capacity_percent 同步下降（int），电池单元温度/电压按公式变化

### Requirement: OSD 云台与相机字段
UAVMqttBridge MUST 组装 payloads（gimbal_pitch/roll/yaw、zoom_factor 从 UAVDroneSim 读取）与 cameras 数组：camera_mode / photo_state / recording_state 使用 int 枚举（0/1），recording_state 与 UAVDroneSim 录像状态一致，并补齐 zoom_factor、ir_zoom_factor、zoom_focus_*、record_time、liveview_world_region、photo/video_storage_settings、screen_split_enable、remain_photo_num、remain_record_duration。

#### Scenario: 云台与相机上报随模拟状态变化
- **WHEN** UAVDroneSim 云台角度/变焦/录像状态变化
- **THEN** OSD payloads 与 cameras 同步输出对应数值与枚举

### Requirement: OSD 顶层结构对齐
UAVMqttBridge MUST 在 OSD data 中补齐 dock report_drone_osd.py 的顶层字段：gear（高度 >8 为 1）、position_state、wind_direction（由朝向按 8 方位枚举推导）/wind_speed、total_flight_distance/total_flight_time（从 UAVDroneSim 累计遥测读取）、speaker、storage、night_lights_state（录像中为 1）、height_limit、distance_limit_status、obstacle_avoidance、rc_lost_action、rth_altitude、total_flight_sorties、exit_wayline_when_rc_lost、country、rid_state、is_near_area_limit、is_near_height_limit、track_id。

#### Scenario: 结构完整可被 dock 解析
- **WHEN** 组件连接且无人机模拟组件已注入
- **THEN** 每周期 OSD 报文包含全部上述字段且类型与 dock 解析一致

### Requirement: 载荷指令分发
UAVMqttBridge MUST 将 services 报文中 payload_authority_grab / camera_mode_switch / camera_photo_take / camera_photo_stop / camera_recording_start / camera_recording_stop / camera_aim / gimbal_reset 分发到 UAVCameraStream.HandleCommand，并把 result 组装 services_reply 发布。

#### Scenario: 载荷指令分发与回包
- **WHEN** 收到 camera_recording_start 的 services 报文
- **THEN** 调用 UAVCameraStream.HandleCommand 并在 services_reply 中返回其 result

### Requirement: 载荷控制源上报
UAVMqttBridge MUST 在 payload_authority_grab 成功后向 thing/product/{无人机SN}/state 发布载荷控制源报文：data {control_source:"A", payloads:[{control_source:"A", payload_index:"52-0-0"}]}（对齐 dock report_control_source.py）。

#### Scenario: 抢权后上报控制源
- **WHEN** payload_authority_grab 返回成功
- **THEN** 发布 thing/product/{无人机SN}/state 载荷控制源报文

### Requirement: 自动返航事件
UAVMqttBridge MUST 绑定 UAVFlightControl.OnReturnHomeStatus，将 rth_auto_trigger 事件拼装为 thing/product/{机场SN}/events 报文（method=return_home_status，data 含 result/status/reason）发布。

#### Scenario: 自动返航事件转发
- **WHEN** 飞控广播 OnReturnHomeStatus（rth_auto_trigger/battery_low）
- **THEN** 发布 events 报文 return_home_status，data 含 result=0/status=rth_auto_trigger/reason=battery_low

