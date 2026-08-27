# mqtt-bridge Specification

## Purpose

UAVMqttBridge 将机场 OSD 输出从精简字段（sn / drone_status）补齐为对齐 dock OsdDock 的完整报文，数值由 UAVDroneSim 状态推导或按模拟基线输出。

## ADDED Requirements

### Requirement: 机场 OSD 字段对齐 OsdDock
UAVMqttBridge MUST 组装机场 OSD data，包含 dock OsdDock 字段集：network_state、drone_in_dock、drone_charge_state、rainfall、wind_speed、environment_temperature、temperature、humidity、latitude、longitude、height、alternate_land_point、first_power_on、position_state、storage、mode_code、cover_state、supplement_light_state、emergency_stop_state、air_conditioner、battery_store_mode、alarm_state、putter_state、sub_device、job_number、acc_time、activation_time、maintain_status、electric_supply_voltage、working_voltage、working_current、backup_battery、drone_battery_maintenance_info、flighttask_step_code、flighttask_prepare_capacity、media_file_detail、wireless_link、drc_state、user_experience_improvement，且字段名与类型与 OsdDock 一致。

#### Scenario: 机场 OSD 结构完整
- **WHEN** 组装机场 OSD data
- **THEN** 输出包含上述全部字段，且子对象字段（alternate_land_point / position_state / storage / air_conditioner / sub_device / backup_battery / drone_battery_maintenance_info / media_file_detail / wireless_link / drone_charge_state / network_state）结构完整

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
