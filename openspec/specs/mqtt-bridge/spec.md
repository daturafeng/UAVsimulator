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
UAVMqttBridge MUST 绑定 UAVFlightControl.OnReturnHomeStatus，将 rth_auto_trigger 事件拼装为 thing/product/{机场SN}/events 报文（method=return_home_info，对齐 dock EventsMethodEnum / ReturnHomeInfo）发布，data 含 planned_path_points（返航航线点数组，元素含 latitude/longitude/height）、last_point_type（有直飞返航点 0、有航线路径 1、无航点 65535）、flight_id；不再发布 return_home_status 报文。

#### Scenario: 自动返航事件转发
- **WHEN** 飞控广播 OnReturnHomeStatus（rth_auto_trigger/battery_low）
- **THEN** 发布 events 报文 return_home_info，data 含 planned_path_points / last_point_type / flight_id，且不发布 method 为 return_home_status 的报文

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

### Requirement: 相机设置指令分发
UAVMqttBridge MUST 将 photo_storage_set / video_storage_set / ir_metering_mode_set / ir_metering_point_set / ir_metering_area_set / poi_mode_enter / poi_mode_exit / poi_circle_speed_set 等载荷指令分发到 UAVCameraStream.HandleCommand，并把 result 组装 services_reply 发布。

#### Scenario: 存储指令分发与回包
- **WHEN** 收到 photo_storage_set 的 services 报文
- **THEN** 调用 UAVCameraStream.HandleCommand 并在 services_reply 中返回其 result

#### Scenario: POI 指令分发与回包
- **WHEN** 收到 poi_mode_enter 的 services 报文
- **THEN** 调用 UAVCameraStream.HandleCommand 并在 services_reply 中返回其 result

### Requirement: OSD 相机设置字段
UAVMqttBridge MUST 在 OSD cameras 中输出实时相机设置：screen_split_enable 为 UAVDroneSim 分屏使能、photo_storage_settings / video_storage_settings 为存储位置数组、zoom_focus_mode / zoom_focus_value 为对焦模式/对焦值（zoom_focus_state 保持 0）。

#### Scenario: 分屏状态实时输出
- **WHEN** UAVDroneSim 分屏使能被设置为 true
- **THEN** OSD cameras.screen_split_enable 输出 true

### Requirement: 机场 OSD 字段对齐 OsdDock
UAVMqttBridge MUST 组装机场 OSD data，包含 dock OsdDock 字段集：network_state、drone_in_dock、drone_charge_state、rainfall、wind_speed、environment_temperature、temperature、humidity、latitude、longitude、height、alternate_land_point、first_power_on、position_state、storage、mode_code、cover_state、supplement_light_state、emergency_stop_state、air_conditioner、battery_store_mode、alarm_state、putter_state、sub_device、job_number、acc_time、activation_time、maintain_status、electric_supply_voltage、working_voltage、working_current、backup_battery、drone_battery_maintenance_info、flighttask_step_code、flighttask_prepare_capacity、media_file_detail、wireless_link、drc_state、user_experience_improvement，且字段名与类型与 OsdDock 一致；maintain_status 含 maintain_status_array 数组（元素含 last_maintain_flight_sorties / last_maintain_time / last_maintain_type / state）。

#### Scenario: 机场 OSD 结构完整
- **WHEN** 组装机场 OSD data
- **THEN** 输出包含上述全部字段，且子对象字段（alternate_land_point / position_state / storage / air_conditioner / sub_device / backup_battery / drone_battery_maintenance_info / media_file_detail / wireless_link / drone_charge_state / network_state / maintain_status）结构完整

#### Scenario: 维护状态基线
- **WHEN** 机场为新机场（未做过保养）
- **THEN** maintain_status 输出 maintain_status_array 数组，首个元素 last_maintain_flight_sorties=0、last_maintain_time=0、last_maintain_type=0（NO）、state=false

#### Scenario: 机场位置与环境基线
- **WHEN** 机场原点为 A、当前时间为 T
- **THEN** latitude/longitude 输出 A 的经纬度、height 输出 12.0、alternate_land_point 输出 A 附近偏移点（safe_land_height=30.0、is_configured=true）、first_power_on 输出 T-180 天、activation_time 输出 T-120 天

### Requirement: 机场状态推导
UAVMqttBridge MUST 从 UAVDroneSim 推导机场状态：drone_in_dock（无人机在机场原点 ±0.00002 度内且高度 ≤12 且处于待机状态）、drone_charge_state（归巢待命且电量 <100 时为充电中，state=1）、flighttask_step_code（按飞行状态映射任务步骤）、acc_time（累计飞行时长取整）、flighttask_prepare_capacity（当前电量取整）、cover_state（归巢待命为 0 否则 1）。

#### Scenario: 无人机在机场内
- **WHEN** 无人机位于机场原点附近且高度 ≤12 且飞行状态为待机
- **THEN** drone_in_dock 输出 true、cover_state 输出 0、drone_charge_state.state 为 1（电量 <100 时）或 0（电量满时）

#### Scenario: 无人机在任务中
- **WHEN** 无人机处于航线任务（Wayline）状态
- **THEN** drone_in_dock 输出 false、cover_state 输出 1、flighttask_step_code 输出 0

#### Scenario: 返航降落任务步骤
- **WHEN** 无人机处于返航（ReturnHome）或降落（Landing）状态
- **THEN** flighttask_step_code 输出 2

### Requirement: 直播能力上报
UAVMqttBridge MUST 在 MQTT 连接成功后向 thing/product/{机场SN}/state 发布直播能力报文，data 为 { live_capacity: { available_video_number, coexist_video_number_max, device_list } }，字段名与结构对齐 dock DockLiveCapacity 与联调脚本 report_live_capacity.py（snake_case）。device_list MUST 含两个设备项：网关设备项（sn=机场SN，camera_list 含 165-0-7 相机，video_list 为 normal-0/normal）与无人机设备项（sn=无人机SN，camera_list 含 176-0-0 普通相机 normal-0/normal 与相机索引相机 normal-0/zoom，可切换类型 normal/wide/zoom/ir）；available_video_number 为视频流总数，coexist_video_number_max 与设备/相机项一致。

#### Scenario: 连接成功后发布直播能力
- **WHEN** MQTT 连接成功
- **THEN** 向 thing/product/{机场SN}/state 发布 data.live_capacity 报文，包含 available_video_number / coexist_video_number_max / device_list

#### Scenario: 直播能力结构完整
- **WHEN** 组装直播能力报文
- **THEN** device_list 含网关与无人机两个设备项，各含 sn / available_video_number / coexist_video_number_max / camera_list；相机项含 camera_index / available_video_number / coexist_video_number_max / video_list；视频项含 video_index / video_type / switchable_video_types

#### Scenario: 无人机主载荷可切换类型
- **WHEN** 组装无人机设备项的相机索引（52-0-0）视频列表
- **THEN** video_type 为 zoom、switchable_video_types 为 [normal, wide, zoom, ir]

### Requirement: DRC 指令通道
UAVMqttBridge MUST 在连接后订阅 thing/product/{机场SN}/drc/down，将报文（tid/bid/method/data，无 gateway）分发到 UAVFlightControl：drone_control / heart_beat / drone_emergency_stop；并把结果回发到 thing/product/{来源SN}/drc/up，data 为 { result, output?: { seq } }（drone_control / heart_beat 带 output.seq，drone_emergency_stop 仅 result）。services 通道 MUST 精确分发 drc_mode_enter / drc_mode_exit（其余 drc_* 前缀方法仍按未知指令回复 services_reply）。

#### Scenario: DRC 指令分发与回执
- **WHEN** 收到 thing/product/{DockSn}/drc/down 的 drone_control 报文
- **THEN** 调用 UAVFlightControl.HandleCommand，并向 thing/product/{来源SN}/drc/up 回发 { tid, bid, timestamp, method, data: { result, output: { seq } } }

#### Scenario: 急停回执
- **WHEN** 收到 thing/product/{DockSn}/drc/down 的 drone_emergency_stop 报文
- **THEN** 回发 drc/up 报文，data 仅含 result（无 output）

#### Scenario: DRC 模式指令走 services
- **WHEN** 收到 method 为 drc_mode_enter / drc_mode_exit 的 services 报文
- **THEN** 调用 UAVFlightControl.HandleCommand 并在 services_reply 中返回其 result

### Requirement: DRC 状态事件上报
UAVMqttBridge MUST 绑定 UAVFlightControl.OnDrcStatusNotify，将 DRC 会话状态变化拼装为 thing/product/{机场SN}/events 报文（method=drc_status_notify）发布，data = { result: 0, drc_state }（对齐 dock DrcStateEnum：0=DISCONNECTED、1=CONNECTING、2=CONNECTED）。

#### Scenario: 进入 DRC 上报已连接
- **WHEN** 飞控广播 OnDrcStatusNotify(2)
- **THEN** 发布 drc_status_notify 事件，data.drc_state=2

#### Scenario: 退出 DRC 上报断开
- **WHEN** 飞控广播 OnDrcStatusNotify(0)
- **THEN** 发布 drc_status_notify 事件，data.drc_state=0

