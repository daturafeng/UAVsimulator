# mqtt-bridge Specification

## Purpose

UAVMqttBridge 机场 OSD 补齐 maintain_status（OsdDockMaintainStatus）字段，使字段集完整对齐 dock OsdDock，dock Jackson 反序列化时维护状态不再为空。

## MODIFIED Requirements

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
