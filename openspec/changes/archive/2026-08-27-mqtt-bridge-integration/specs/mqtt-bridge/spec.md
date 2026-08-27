# mqtt-bridge Specification

## Purpose

打通 UAVsimulator 与 dock（大疆上云 API）之间的 MQTT 通道：模拟器扮演"机场 + 无人机"角色，订阅 dock 下发的 services 指令并执行，回发 services_reply 与 events，周期上报 OSD 遥测与设备状态。首期设备：机场 DOCK3TEST001、无人机 1581F8HGXTEST001（M4TD，相机索引 52-0-0）。

## ADDED Requirements

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
