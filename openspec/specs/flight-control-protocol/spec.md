# flight-control-protocol Specification

## Purpose
定义 UAVsimulator 对上云 API 飞控指令的解析、状态机与事件模型，使模拟器能按大疆 Cloud API 协议接收并执行 dock 下发的飞行控制指令（M4TD + dock3）。
## Requirements
### Requirement: 指令协议映射
系统 MUST 支持解析上云 API 的 services 指令 method，至少覆盖：`flight_authority_grab`、`takeoff_to_point`、`flighttask_create`、`flighttask_prepare`、`flighttask_execute`、`flighttask_undo`、`flighttask_pause`、`flighttask_recovery`、`return_home`、`return_home_cancel`、`live_start_push`、`live_stop_push`、`live_set_quality`、`live_lens_change`；未知 method 返回解析失败。

#### Scenario: 识别已知指令
- **WHEN** 收到 `takeoff_to_point` 指令报文
- **THEN** 系统正确解析出目标经纬度、目标高度、安全起飞高度、返航高度、最大速度等参数，并进入起飞到点状态

#### Scenario: 未知指令
- **WHEN** 收到未支持的 method
- **THEN** 解析返回失败，且不改变当前飞控状态

### Requirement: 飞控状态机
系统 MUST 维护飞控状态机：待机（Idle）、起飞到点（TakingOff）、航线飞行（Wayline）、返航（ReturnHome）、降落（Landing）。飞控权抢占成功后才能执行起飞/航线指令；任务执行期间收到返航指令应中断当前任务。

#### Scenario: 飞控权抢占
- **WHEN** 收到 `flight_authority_grab` 且返回业务 bid
- **THEN** 系统进入可执行任务状态，可接受起飞或航线指令

#### Scenario: 任务中返航
- **WHEN** 航线飞行中收到 `return_home`
- **THEN** 当前任务被中断，系统进入返航状态

### Requirement: 指令结果与事件上报模型
系统 MUST 为每条 services 指令生成结果（result=0 成功，非 0 失败）与对应的事件（`takeoff_to_point_progress`、`flighttask_progress`、`flighttask_ready`），事件携带进度字段（如 status、progress、current_waypoint_index）供桥接层转发；flighttask_prepare 处理成功后 MUST 广播任务就绪事件 flighttask_ready（携带该任务 flight_id）。

#### Scenario: 起飞到点进度事件
- **WHEN** 起飞到点任务执行中发生阶段变化（起飞/飞行/到达）
- **THEN** 系统产生 `takeoff_to_point_progress` 事件，包含当前状态与进度

#### Scenario: 任务就绪事件
- **WHEN** flighttask_prepare 指令处理成功
- **THEN** 系统广播 flighttask_ready 事件，携带该任务的 flight_id

#### Scenario: 指令结果
- **WHEN** 任意 services 指令处理完成
- **THEN** 系统返回 `result` 字段，0 表示成功、非 0 表示失败

### Requirement: 直播会话模型
系统 MUST 维护直播会话：根据 `live_start_push` 生成 video_id（格式 `{droneSn}/{cameraIndex}/{videoType}-0`）与 RTMP 推流地址（格式 `rtmpBaseUrl + {droneSn}-{cameraIndex}`），支持停止推流、切换清晰度与镜头；同一视频流重复开启应视为已存在。

#### Scenario: 开始推流
- **WHEN** 收到 `live_start_push` 且 url_type=1（RTMP）
- **THEN** 系统记录该 video_id 为推流中，并生成对应 RTMP 地址

#### Scenario: 停止推流
- **WHEN** 收到 `live_stop_push` 且 video_id 匹配
- **THEN** 系统将该视频流标记为停止

### Requirement: 低电量自动返航
UAVFlightControl MUST 绑定 UAVDroneSim.OnBatteryLow：电量低于返航阈值、持有飞控权且当前处于空中状态（起飞/航线/巡航）时自动执行返航（复用 return_home 路径，停止当前任务并飞回机场返航高度），返回中不重复触发；返航触发广播 OnReturnHomeStatus（status=rth_auto_trigger、reason=battery_low）。

#### Scenario: 低电量自动返航
- **WHEN** 航线飞行中电量降至返航阈值以下
- **THEN** 当前任务被中断，进入返航状态并广播 rth_auto_trigger 事件

#### Scenario: 返航中不重复触发
- **WHEN** 已在返航状态且电量继续下降
- **THEN** 不再次触发自动返航

#### Scenario: 待机低电量不返航
- **WHEN** 待机状态电量降至阈值以下
- **THEN** 不触发自动返航（无飞行任务）
