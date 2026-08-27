# flight-control-protocol Delta Spec

## MODIFIED Requirements

### Requirement: 指令结果与事件上报模型
系统 MUST 为每条 services 指令生成结果（result=0 成功，非 0 失败）与对应的事件（takeoff_to_point_progress、flighttask_progress、flighttask_ready），事件携带进度字段（如 status、progress、current_waypoint_index）供桥接层转发；flighttask_prepare 处理成功后 MUST 广播任务就绪事件 flighttask_ready（携带该任务 flight_id）。

#### Scenario: 起飞到点进度事件
- **WHEN** 起飞到点任务执行中发生阶段变化（起飞/飞行/到达）
- **THEN** 系统产生 takeoff_to_point_progress 事件，包含当前状态与进度

#### Scenario: 任务就绪事件
- **WHEN** flighttask_prepare 指令处理成功
- **THEN** 系统广播 flighttask_ready 事件，携带该任务的 flight_id

#### Scenario: 指令结果
- **WHEN** 任意 services 指令处理完成
- **THEN** 系统返回 result 字段，0 表示成功、非 0 表示失败
