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
UAVMqttBridge MUST 组装机场 OSD data，包含 dock OsdDock 字段集：network_state、drone_in_dock、drone_charge_state、rainfall、wind_speed、environment_temperature、temperature、humidity、latitude、longitude、height、alternate_land_point、first_power_on、position_state、storage、mode_code、cover_state、supplement_light_state、emergency_stop_state、air_conditioner、battery_store_mode、alarm_state、putter_state、sub_device、job_number、acc_time、activation_time、maintain_status、electric_supply_voltage、working_voltage、working_current、backup_battery、drone_battery_maintenance_info、flighttask_step_code、flighttask_prepare_capacity、media_file_detail、wireless_link、drc_state、user_experience_improvement，且字段名与类型与 OsdDock 一致；maintain_status 含 maintain_status_array 数组（元素含 last_maintain_flight_sorties / last_maintain_time / last_maintain_type / state）。以下字段 MUST 由远程调试指令驱动：supplement_light_state（supplement_light_open/close）、alarm_state（alarm_state_switch）、putter_state（putter_open/close，0/1）、battery_store_mode（battery_store_mode_switch，1/2）、air_conditioner.air_conditioner_state（air_conditioner_mode_switch，0-3）、drone_battery_maintenance_info.maintenance_state（battery_maintenance_switch，0/1）、wireless_link.link_workmode（sdr_workmode_switch，0/1）、sub_device.device_online_status（drone_open/close，电源开 1 / 关 0）、mode_code（调试模式激活时为 REMOTE_DEBUGGING=2，否则沿用任务/待机推导 4/3）。

#### Scenario: 机场 OSD 结构完整
- **WHEN** 组装机场 OSD data
- **THEN** 输出包含上述全部字段，且子对象字段（alternate_land_point / position_state / storage / air_conditioner / sub_device / backup_battery / drone_battery_maintenance_info / media_file_detail / wireless_link / drone_charge_state / network_state / maintain_status）结构完整

#### Scenario: 维护状态基线
- **WHEN** 机场为新机场（未做过保养）
- **THEN** maintain_status 输出 maintain_status_array 数组，首个元素 last_maintain_flight_sorties=0、last_maintain_time=0、last_maintain_type=0（NO）、state=false

#### Scenario: 机场位置与环境基线
- **WHEN** 机场原点为 A、当前时间为 T
- **THEN** latitude/longitude 输出 A 的经纬度、height 输出 12.0、alternate_land_point 输出 A 附近偏移点（safe_land_height=30.0、is_configured=true）、first_power_on 输出 T-180 天、activation_time 输出 T-120 天

#### Scenario: 设备状态随调试指令联动
- **WHEN** 依次执行 supplement_light_open、alarm_state_switch(action=1)、putter_open、battery_store_mode_switch(action=2)、air_conditioner_mode_switch(action=1)、battery_maintenance_switch(action=1)、sdr_workmode_switch(linkWorkmode=0)、debug_mode_open、drone_close
- **THEN** OSD 输出 supplement_light_state=true、alarm_state=true、putter_state=1、battery_store_mode=2、air_conditioner.air_conditioner_state=1、drone_battery_maintenance_info.maintenance_state=1、wireless_link.link_workmode=0、mode_code=2、sub_device.device_online_status=0

#### Scenario: 调试模式影响模式编码
- **WHEN** 执行 debug_mode_open 且机场处于待机
- **THEN** mode_code 输出 2（REMOTE_DEBUGGING）；执行 debug_mode_close 后恢复待机推导值 3

### Requirement: 机场状态推导
UAVMqttBridge MUST 从 UAVDroneSim 推导机场状态：drone_in_dock（无人机在机场原点 ±0.00002 度内且高度 ≤12 且处于待机状态）、drone_charge_state（归巢待命且电量 <100 时为充电中，state=1）、flighttask_step_code（按飞行状态映射任务步骤）、acc_time（累计飞行时长取整）、flighttask_prepare_capacity（当前电量取整）、cover_state（归巢待命为 0 否则 1）。远程调试指令对设备状态 MUST 具有覆盖优先级：cover_open / cover_close 覆盖 cover_state（1/0），charge_open / charge_close 覆盖 drone_charge_state.state（1/0），指令未执行过时回退到推导值。

#### Scenario: 无人机在机场内
- **WHEN** 无人机位于机场原点附近且高度 ≤12 且飞行状态为待机
- **THEN** drone_in_dock 输出 true、cover_state 输出 0、drone_charge_state.state 为 1（电量 <100 时）或 0（电量满时）

#### Scenario: 无人机在任务中
- **WHEN** 无人机处于航线任务（Wayline）状态
- **THEN** drone_in_dock 输出 false、cover_state 输出 1、flighttask_step_code 输出 0

#### Scenario: 返航降落任务步骤
- **WHEN** 无人机处于返航（ReturnHome）或降落（Landing）状态
- **THEN** flighttask_step_code 输出 2

#### Scenario: 远程调试覆盖舱门状态
- **WHEN** 收到 cover_open 且无人机归巢待命
- **THEN** cover_state 输出 1（指令覆盖优先于归巢推导）；收到 cover_close 后 cover_state 输出 0

#### Scenario: 远程调试覆盖充电状态
- **WHEN** 收到 charge_open
- **THEN** drone_charge_state.state 输出 1；收到 charge_close 后 state 输出 0（与电量推导无关）

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

### Requirement: 远程调试指令分发
UAVMqttBridge MUST 将 services 通道收到的远程调试指令精确分发并处理，覆盖 DebugMethodEnum 的 20 个方法：debug_mode_open / debug_mode_close / supplement_light_open / supplement_light_close / device_reboot / drone_open / drone_close / drone_format / device_format / cover_open / cover_close / putter_open / putter_close / charge_open / charge_close / battery_maintenance_switch / alarm_state_switch / battery_store_mode_switch / sdr_workmode_switch / air_conditioner_mode_switch。带参数的指令 MUST 解析 data 并校验：battery_maintenance_switch / alarm_state_switch 的 action 为 0/1（SwitchActionEnum），battery_store_mode_switch 的 action 为 1/2（BatteryStoreModeEnum），sdr_workmode_switch 的 linkWorkmode 为 0/1（LinkWorkModeEnum），air_conditioner_mode_switch 的 action 为 0/1/2/3（AirConditionerModeSwitchActionEnum）；参数缺失或越界 MUST 返回 InvalidParams。处理成功后 MUST 回发 services_reply，data 为 { result: 0, output: { status: "sent" } }（对齐 ServicesReplyData<RemoteDebugResponse>）。

#### Scenario: 无参调试指令分发
- **WHEN** 收到 method 为 cover_open / charge_close / drone_open 等的 services 报文
- **THEN** 指令执行成功并回发 services_reply，data 含 result=0 与 output.status="sent"

#### Scenario: 开关类指令参数校验
- **WHEN** 收到 alarm_state_switch 且 data.action 为 0 或 1
- **THEN** 执行成功并更新报警状态；data.action 为其他值或缺失时回发 result=InvalidParams

#### Scenario: 模式类指令参数校验
- **WHEN** 收到 air_conditioner_mode_switch 且 data.action 为 0/1/2/3
- **THEN** 执行成功并更新空调模式；action 越界或缺失时回发 result=InvalidParams

### Requirement: 远程调试进度事件
UAVMqttBridge MUST 对带进度的方法（device_reboot / drone_open / drone_close / drone_format / device_format / cover_open / cover_close / putter_open / putter_close / charge_open / charge_close）发布 RemoteDebugProgress 进度事件：先发布 status="sent"（percent=0），再发布 status="ok"（percent=100），method 与 services 指令同名，data 为 { result: 0, output: { status, progress: { percent, currentStep, totalSteps, stepKey, stepResult } } }（对齐 EventsDataRequest<RemoteDebugProgress>）；有明确步骤语义的方法 MUST 携带对应 stepKey（cover_open=open_cover、cover_close=close_cover、putter_open=free_putter、putter_close=close_putter、charge_close=stop_charge、drone_open=open_drone），其余方法 stepKey 缺省。switch 类无进度方法（battery_maintenance_switch 等）MUST NOT 发布进度事件。

#### Scenario: 舱门开闭进度
- **WHEN** 收到 cover_open 指令
- **THEN** 先发布 cover_open 事件 status="sent"、progress.percent=0、stepKey="open_cover"，再发布 status="ok"、progress.percent=100

#### Scenario: 无进度方法不发布事件
- **WHEN** 收到 debug_mode_open / alarm_state_switch 等 switch 类指令
- **THEN** 仅回发 services_reply，不发布任何进度事件

### Requirement: 固件升级指令分发
UAVMqttBridge MUST 将 services 通道收到的固件升级指令 ota_create 精确分发并处理（对齐 dock FirmwareMethodEnum.OTA_CREATE）。指令 data MUST 解析 devices 数组（1-2 个设备），每个设备项 MUST 含 sn / product_version / file_url / md5 / file_size / firmware_upgrade_type / file_name；设备项缺失或字段类型非法时 MUST 返回 InvalidParams。处理成功后 MUST 回发 services_reply，data 为 { result: 0, output: { status: "sent" } }（对齐 ServicesReplyData<OtaCreateResponse>），并记录待升级设备（机场与/或无人机）。

#### Scenario: 固件升级指令分发
- **WHEN** 收到 method 为 ota_create 的 services 报文，data.devices 含机场与无人机各一项（firmware_upgrade_type 为 2/3）
- **THEN** 指令执行成功并回发 services_reply，data 含 result=0 与 output.status="sent"

#### Scenario: 升级设备参数非法
- **WHEN** 收到 ota_create 且 data.devices 为空、设备项缺 sn/product_version，或 firmware_upgrade_type 不在 2/3
- **THEN** 回发 result=InvalidParams 的 services_reply，不发布进度事件

### Requirement: 固件升级进度事件
UAVMqttBridge MUST 在 ota_create 成功后向 thing/product/{机场SN}/events 发布 ota_progress 进度事件（对齐 dock EventsMethodEnum.OTA_PROGRESS / EventsDataRequest<OtaProgress>），data 为 { result: 0, output: { status, progress: { percent, current_step }, ext: { rate } } }。事件序列 MUST 依次为 status="sent"（percent=0、current_step=1、rate=0）、status="in_progress"（percent=50、current_step=1）、status="ok"（percent=100、current_step=2、rate=0）；current_step 对齐 OtaProgressStepEnum（1=DOWNLOADING、2=UPGRADING）。

#### Scenario: 升级进度事件序列
- **WHEN** ota_create 指令处理成功
- **THEN** 依次发布 ota_progress 事件 sent（percent=0, current_step=1, rate=0）、in_progress（percent=50, current_step=1）、ok（percent=100, current_step=2），三条事件 data.result 均为 0

#### Scenario: 升级失败不发布进度
- **WHEN** ota_create 参数校验失败（result 非 0）
- **THEN** 仅回发 services_reply，不发布任何 ota_progress 事件

### Requirement: 固件版本 state 上报
UAVMqttBridge MUST 在 MQTT 连接成功后向 thing/product/{机场SN}/state 发布固件版本报文（对齐 dock DockStateDataKeyEnum.FIRMWARE_VERSION / DockFirmwareVersion）：data 含 firmware_version（格式 xx.xx.xxxx）、compatible_status（是否需要一致性升级，默认 false）、firmware_upgrade_status（升级中 true，否则 false）；向 thing/product/{无人机SN}/state 发布 rc_and_drone_firmware_version（对齐 RcStateDataKeyEnum.FIRMWARE_VERSION / FirmwareVersion，data 为 { firmware_version }）与载荷固件版本（对齐 PayloadFirmwareVersion，data 键为载荷索引，值为 { firmware_version }）。OTA 升级成功（ok 事件）后 MUST 将机场与无人机 firmware_version 更新为目标版本并重发固件版本 state，firmware_upgrade_status 恢复 false。

#### Scenario: 连接成功后固件版本上报
- **WHEN** MQTT 连接成功
- **THEN** 向机场 state 发布 firmware_version 报文（含 firmware_version / compatible_status / firmware_upgrade_status），向无人机 state 发布 rc_and_drone_firmware_version 与载荷固件版本报文

#### Scenario: 升级完成后版本更新
- **WHEN** ota_create 指定目标版本且 ok 进度事件已发布
- **THEN** 机场与无人机 state 重发 firmware_version 报文，firmware_version 更新为目标版本、firmware_upgrade_status=false

### Requirement: 物模型属性设置订阅与回执
UAVMqttBridge MUST 在 MQTT 连接成功后订阅 thing/product/{机场SN}/property/set（对齐 dock TopicConst 物模型属性设置通道），解析报文的 tid/bid 与 data 单属性对象（{属性名: {字段: 值}}），按属性名分发校验，并回发 thing/product/{机场SN}/property/set_reply（对齐 TopicPropertySetResponse）：data 为 { result: 0 }（成功，PropertySetReplyResultEnum.SUCCESS）或 { result: 1 }（参数非法，FAILED）；报文头含 tid/bid/timestamp。未知属性或报文缺 data 时 MUST 回 result=1。

#### Scenario: 属性设置成功回执
- **WHEN** 收到 property/set 报文，data 为合法属性对象（如 { height_limit: { height_limit: 120 } }）
- **THEN** 属性状态更新并回发 property/set_reply，data.result=0

#### Scenario: 属性参数非法回执
- **WHEN** 收到 property/set 报文，data 属性值越界（如 height_limit=10）、缺必填字段或属性名未知
- **THEN** 不更新状态，回发 property/set_reply，data.result=1

### Requirement: 无人机属性设置与校验
UAVMqttBridge MUST 支持 7 个无人机属性（对齐 dock PropertySetFieldEnum / Receiver.valid）：night_lights_state（0/1）、height_limit（20-1500 米）、distance_limit_status（state 0/1 与 distance_limit 15-8000 至少一项，值域校验）、obstacle_avoidance（horizon/upside/downside 0/1 至少一项）、rth_altitude（20-500 米）、rc_lost_action（0=HOVER/1=LAND/2=RETURN_HOME）、exit_wayline_when_rc_lost（0=CONTINUE/1=EXECUTE_RC_LOST_ACTION）。校验通过后 MUST 写入无人机属性状态。

#### Scenario: 高度限制设置
- **WHEN** 收到 { height_limit: { height_limit: 120 } }
- **THEN** 无人机属性状态 HeightLimit 更新为 120 且回执 result=0

#### Scenario: 高度限制越界
- **WHEN** 收到 { height_limit: { height_limit: 10 } }（小于 20）
- **THEN** 属性状态不变，回执 result=1

#### Scenario: 距离限制状态设置
- **WHEN** 收到 { distance_limit_status: { state: 1, distance_limit: 3000 } }
- **THEN** 无人机属性状态 DistanceLimitState/DistanceLimit 更新且回执 result=0

#### Scenario: 避障设置
- **WHEN** 收到 { obstacle_avoidance: { horizon: 0, upside: 1, downside: 1 } }
- **THEN** 无人机属性状态避障三项更新且回执 result=0

#### Scenario: 夜航灯/返航高度/失控动作设置
- **WHEN** 收到 night_lights_state（0/1）、rth_altitude（20-500）、rc_lost_action（0/1/2）合法值
- **THEN** 对应属性状态更新且回执 result=0

### Requirement: 机场属性设置
UAVMqttBridge MUST 支持机场属性 user_experience_improvement（0/1/2，对齐 PropertySetEnum.USER_EXPERIENCE_IMPROVEMENT），校验通过后写入机场属性状态。

#### Scenario: 用户体验改进计划设置
- **WHEN** 收到 { user_experience_improvement: { user_experience_improvement: 1 } }
- **THEN** 机场属性状态 UserExperienceImprovement 更新为 1 且回执 result=0

#### Scenario: 用户体验改进计划越界
- **WHEN** 收到 { user_experience_improvement: { user_experience_improvement: 9 } }
- **THEN** 机场属性状态不变，回执 result=1

### Requirement: OSD 属性联动
UAVMqttBridge MUST 在组装无人机 OSD 时使用属性状态替代硬编码：night_lights_state、height_limit、distance_limit_status（state/distance_limit/is_near_distance_limit）、obstacle_avoidance（horizon/upside/downside）、rc_lost_action、rth_altitude、exit_wayline_when_rc_lost；组装机场 OSD 时 user_experience_improvement 使用属性状态。默认值与属性状态结构默认值一致。

#### Scenario: 属性设置后 OSD 联动
- **WHEN** 设置 height_limit=120 且 rth_altitude=100 后重新组装无人机 OSD
- **THEN** OSD data.height_limit=120、data.rth_altitude=100

#### Scenario: 默认属性值输出
- **WHEN** 未收到任何属性设置时组装 OSD
- **THEN** 无人机 OSD height_limit=500、distance_limit_status.distance_limit=3000、obstacle_avoidance 全 1、rth_altitude=60、rc_lost_action=2、exit_wayline_when_rc_lost=1、night_lights_state=0；机场 OSD user_experience_improvement=2

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

### Requirement: 设备主动 requests 通道
UAVMqttBridge MUST 在 MQTT 连接成功后订阅 thing/product/{机场SN}/requests_reply，并能向 thing/product/{机场SN}/requests 发布设备主动请求。请求报文 MUST 含非空 tid / bid、毫秒时间戳、gateway=机场SN、method 与 data；每个请求 MUST 以 tid / bid / method 记录为待处理请求。requests_reply 仅在 tid / bid / method 均与待处理请求一致时更新业务状态并完成该请求；未知 bid 或字段不匹配的响应 MUST 被忽略且不得更新业务状态。

#### Scenario: 主动请求报文结构
- **WHEN** 模拟器发布 method=config、data={config_type:"json", config_scope:"product"} 的请求
- **THEN** thing/product/{机场SN}/requests 报文含非空 tid / bid、timestamp、gateway=机场SN、method=config 与原始 data

#### Scenario: 响应关联成功
- **WHEN** requests_reply 的 tid / bid / method 与待处理请求完全一致
- **THEN** 模拟器按 method 解析响应、移除待处理请求并广播请求完成事件

#### Scenario: 未知或错配响应
- **WHEN** requests_reply 的 bid 不存在，或 tid / method 与待处理请求不一致
- **THEN** 模拟器忽略该响应，不移除原待处理请求且不更新任何业务状态

### Requirement: 上线配置与绑定状态请求
UAVMqttBridge MUST 在 MQTT 连接成功且 requests_reply 订阅完成后发布 config 与 airport_bind_status 两个启动请求。config 的 data MUST 为 { config_type: "json", config_scope: "product" }；airport_bind_status 的 data.devices MUST 含机场 SN 与无人机 SN 两项。config 成功响应 data MUST 解析 ntp_server_host / app_id / app_key / app_license；airport_bind_status 成功响应 MUST 解析 data={result:0, output:{bind_status:[...]}}，每项按 sn 记录 is_device_bind_organization / organization_id / organization_name / device_callsign。

#### Scenario: 连接成功启动请求
- **WHEN** MQTT 连接成功且 requests_reply 已订阅
- **THEN** 模拟器依次发布 config 与 airport_bind_status 请求，绑定状态请求的 devices 含机场和无人机 SN

#### Scenario: 配置响应落地
- **WHEN** 收到匹配的 config 响应，data 含 ntp_server_host / app_id / app_key / app_license
- **THEN** 模拟器记录最新产品配置并广播 config 请求成功

#### Scenario: 绑定状态响应落地
- **WHEN** 收到匹配的 airport_bind_status 响应，data.result=0 且 output.bind_status 含机场与无人机状态
- **THEN** 模拟器按 SN 记录两台设备的组织绑定状态、组织标识、组织名称与呼号

### Requirement: 未绑定设备组织握手
UAVMqttBridge MUST 在 airport_bind_status 成功响应表明机场或无人机未绑定时，仅当 device_binding_code 非空才发布 airport_organization_get，请求 data 含 device_binding_code 与 organization_id。airport_organization_get 成功响应 data={result:0, output:{organization_name}} 后，MUST 发布 airport_organization_bind，请求 data.bind_devices 含机场与无人机两项；每项含 device_binding_code / organization_id / device_callsign / sn / device_model_key，默认机场 model key 为 "3-3-0"、无人机 model key 为 "0-100-1"。airport_organization_bind 成功响应 MUST 解析 output.err_infos，只有两台设备 err_code 均为 0 时组织绑定状态才标记成功。

#### Scenario: 未绑定时查询组织
- **WHEN** airport_bind_status 返回任一设备未绑定且 device_binding_code 已配置
- **THEN** 模拟器发布 airport_organization_get 请求，data 使用配置的绑定码与组织标识

#### Scenario: 未配置绑定码
- **WHEN** airport_bind_status 返回未绑定但 device_binding_code 为空
- **THEN** 模拟器不发布 airport_organization_get 或 airport_organization_bind

#### Scenario: 查询组织后发起绑定
- **WHEN** airport_organization_get 匹配响应 data.result=0 且 output.organization_name 非空
- **THEN** 模拟器记录组织名称，并发布包含机场 "3-3-0" 与无人机 "0-100-1" 的 airport_organization_bind 请求

#### Scenario: 组织绑定成功
- **WHEN** airport_organization_bind 匹配响应 data.result=0，output.err_infos 含机场与无人机且每项 err_code=0
- **THEN** 模拟器将机场与无人机组织绑定状态标记为成功并广播请求完成

### Requirement: 对象存储配置请求
UAVMqttBridge MUST 提供显式 storage_config_get 请求入口，请求 data 为 { module: 0 }（MEDIA）。成功响应 MUST 解析 data={result:0, output:{bucket, credentials, endpoint, object_key_prefix, provider, region}} 并记录最新对象存储配置；result 非 0 或 output 缺少必填字段时 MUST 广播请求失败且不得覆盖最近一次成功配置。

#### Scenario: 请求媒体对象存储配置
- **WHEN** 调用对象存储配置请求入口
- **THEN** 模拟器向 requests topic 发布 method=storage_config_get、data.module=0 的请求

#### Scenario: 对象存储配置成功
- **WHEN** 收到匹配的 storage_config_get 响应，data.result=0 且 output 必填字段齐全
- **THEN** 模拟器记录 bucket / credentials / endpoint / object_key_prefix / provider / region 并广播请求成功

#### Scenario: 对象存储配置失败
- **WHEN** storage_config_get 响应 result 非 0 或 output 字段缺失
- **THEN** 模拟器广播请求失败且保留最近一次成功的对象存储配置
