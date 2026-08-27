# mqtt-bridge Specification

## Purpose

OSD 遥测报文的电量、云台、摄像头字段从 UAVDroneSim 载荷状态读取，字段结构对齐 dock report_drone_osd.py（双电池、int 枚举、完整 cameras 与顶层结构）。

## ADDED Requirements

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
