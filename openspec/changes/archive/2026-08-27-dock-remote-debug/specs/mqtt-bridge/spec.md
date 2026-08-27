## ADDED Requirements

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

## MODIFIED Requirements

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
