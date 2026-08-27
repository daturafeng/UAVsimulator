# flight-control-protocol Delta Spec

## ADDED Requirements

### Requirement: 指点飞行指令状态机
系统 MUST 支持指点飞行三件套指令：fly_to_point（飞向目标点）、fly_to_point_stop（停止指点飞行）、fly_to_point_update（更新目标点与速度）。处理前 MUST 校验飞控权；执行时中断当前任务并驱动无人机飞向目标点；指令成功返回 result=0。

#### Scenario: 指点飞行启动
- **WHEN** 收到 fly_to_point，data 含 fly_to_id / max_speed（1-15）/ points[0]（latitude/longitude/height，height 2-10000）且已抢占飞控权
- **THEN** 中断当前任务，无人机飞向 points[0]，状态迁移至空中巡航（Flying），并广播 fly_to_point_progress（status=wayline_progress，携带 fly_to_id）

#### Scenario: 指点飞行停止
- **WHEN** 收到 fly_to_point_stop 且当前处于指点飞行会话
- **THEN** 停止任务，广播 fly_to_point_progress（status=wayline_cancel）

#### Scenario: 指点飞行更新
- **WHEN** 收到 fly_to_point_update（无 fly_to_id），data 含 max_speed / points[0] 且当前处于指点飞行会话
- **THEN** 更新目标点与速度，无人机飞向新目标，广播 fly_to_point_progress（status=wayline_progress）

#### Scenario: 指点飞行完成
- **WHEN** 指点飞行航点到达完成
- **THEN** 广播 fly_to_point_progress（status=wayline_ok）

#### Scenario: 参数与状态校验
- **WHEN** 未抢占飞控权、points 为空、height 越界（<2 或 >10000）、max_speed 越界（<1 或 >15）或状态冲突（任务中）
- **THEN** 返回对应非 0 result（NoAuthority / InvalidParams / StateConflict），不启动指点飞行
