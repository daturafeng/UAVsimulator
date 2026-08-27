## Context

参见 proposal.md - Why：fly_to_point 三件套指令缺失，dock ControlMethodEnum 支持但模拟器统一回 UnknownMethod；fly_to_point_progress 事件缺失，dock SDKControlService.flyToPointProgress 无法收到进度。

现状：飞控层 UUAVFlightControlComponent.HandleCommand 按 method 分发（flight_authority_grab / takeoff_to_point / flighttask_* / return_home_*），无 fly_ 分支；桥接层 DispatchServicesMessage 的飞控指令前缀为 flight_/takeoff_/return_home，fly_to_point 落入未知指令回非 0；事件发布链路已具备（PublishEvent + kEvent* 常量 + 动态委托绑定），可直接复用。

## Goals / Non-Goals

**Goals:**
- 实现 fly_to_point / fly_to_point_stop / fly_to_point_update 三条指令的解析、校验与执行，驱动无人机飞向目标点。
- 新增 fly_to_point_progress 事件上报链路（飞控委托 + 桥接层转发 + Build* 测试入口）。
- 自动化测试覆盖指令结果码、状态迁移与事件报文结构。

**Non-Goals:**
- 不实现多航点指点飞行（M30 系列仅支持单点，取 points[0]）。
- 不改变既有 takeoff_to_point / flighttask_* / return_home_* 行为。
- 不做与协议无关的重构。

## Decisions

1. **method 常量集中定义**：在 UAV::CloudApi 新增 kMethodFlyToPoint / kMethodFlyToPointStop / kMethodFlyToPointUpdate / kEventFlyToPointProgress，与现有常量体系一致。
   替代方案：桥接层内联字符串 method——与现有常量体系不一致，弃用。

2. **指点飞行状态复用 Flying**：指点飞行复用空中巡航状态（EUAVFlightState::Flying），不新增枚举；新增成员 bFlyToActive 标记活动会话（记录 fly_to_id）。
   替代方案：新增 EUAVFlightState::FlyTo——增加状态枚举且需同步 OSD mode_code 映射，弃用。

3. **事件报文结构**：fly_to_point_progress data = { result: 0, status, fly_to_id, way_point_index }，对齐 dock FlyToPointProgress（Jackson snake_case）；status 使用 FlyToStatusEnum：wayline_progress（启动/到达中间点）、wayline_ok（完成）、wayline_cancel（停止）、wayline_failed（保留，当前模拟不触发）。

4. **指令语义**：fly_to_point 校验飞控权 + points 非空 + 经纬度/高度合法（height 2-10000 按 dock Point 校验），中断当前任务（DroneSim->StopMission）后飞向 points[0]，max_speed 1-15 覆盖 MaxHorizontalSpeed；fly_to_point_update 无 fly_to_id，复用 bFlyToActive 会话（未在指点飞行返回 StateConflict），更新目标与速度；fly_to_point_stop 无 data，停止任务并广播 wayline_cancel。

5. **测试入口**：桥接层新增 public BuildFlyToPointProgressEventData（status/fly_to_id/way_point_index/result），沿用 BuildFlighttaskProgressEventData 模式；飞控指令测试直接 NewObject 组件 + SetDroneSim 调 HandleCommand 断言结果码与状态，新增 UAVFlyToPointTests.cpp。

## Risks / Trade-offs

- [单点限制] dock 要求 M30 仅单点，模拟器取 points[0]，多点场景（如 M350）后续按 dock 校验扩展。
- [状态冲突] 指点飞行与 takeoff_to_point / flighttask 等任务互斥，冲突时返回 StateConflict 而非强制抢占，与 dock 设备语义一致。
- [max_speed 边界] dock 校验 1-15，模拟器同样校验，越界返回 InvalidParams。

## Migration Plan

实现并补齐测试（Automation RunTests UAV）→ openspec validate --specs → 归档 → git commit + push。模拟器无持久化数据，无需数据迁移；联调若发现字段不符，回退为修改 BuildFlyToPointProgressEventData 后重新发布。

## Open Questions

无。
