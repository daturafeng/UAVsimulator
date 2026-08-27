## ADDED Requirements

### Requirement: DRC 摇杆直控会话
系统 MUST 支持 DRC 虚拟摇杆直控会话：drc_mode_enter 校验飞控权与空中状态后进入会话（成功返回 result=0 并广播 OnDrcStatusNotify(2)）；drc_mode_exit 退出会话并广播 OnDrcStatusNotify(0)；会话期间才允许 drone_control / heart_beat。

#### Scenario: 进入 DRC 会话
- **WHEN** 收到 drc_mode_enter 且已抢占飞控权、无人机处于空中（非待机）
- **THEN** 进入 DRC 会话，返回 result=0，并广播 drc_status_notify（drc_state=2）

#### Scenario: 进入 DRC 前置校验
- **WHEN** 未抢占飞控权或无人机处于待机状态收到 drc_mode_enter
- **THEN** 返回对应非 0 result（NoAuthority / StateConflict），不进入会话

#### Scenario: 退出 DRC 会话
- **WHEN** 收到 drc_mode_exit 且当前处于 DRC 会话
- **THEN** 退出会话、停止摇杆控制，返回 result=0，并广播 drc_status_notify（drc_state=0）

### Requirement: 摇杆控制与心跳指令
系统 MUST 在 DRC 会话中处理 drone_control（seq/x/y/h/w/freq/delayTime，x∈[-17,17]、y∈[-17,17]、h∈[-4,5]、w∈[-90,90]、freq∈[2,10]、delayTime∈[100,1000]）：停止现有任务并驱动无人机按摇杆移动；heart_beat 更新心跳序号；drone_emergency_stop 停止一切运动；参数越界或缺 seq 返回 InvalidParams，非会话返回 StateConflict。

#### Scenario: 摇杆控制
- **WHEN** 收到合法 drone_control 且处于 DRC 会话
- **THEN** 中断现有任务，无人机按机体坐标摇杆速度移动，返回 result=0 并记录该 seq 供回执

#### Scenario: 心跳回执
- **WHEN** 收到 heart_beat 且处于 DRC 会话
- **THEN** 记录心跳序号并返回 result=0

#### Scenario: 急停
- **WHEN** 收到 drone_emergency_stop
- **THEN** 停止全部运动与任务，返回 result=0

#### Scenario: 参数与状态校验
- **WHEN** drone_control 参数越界、缺 seq 或未处于 DRC 会话
- **THEN** 返回对应非 0 result（InvalidParams / StateConflict），不驱动运动
