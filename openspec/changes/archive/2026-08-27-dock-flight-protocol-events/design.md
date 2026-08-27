## Context

参见 proposal.md - Why：flighttask_progress 报文结构错位导致 dock 任务状态机失效，return_home_status 不是 dock 支持的 method，flighttask_ready 与 hms 事件缺失。

现状：事件 method 常量集中在 UAV::CloudApi（UAVCloudApiTypes.h/.cpp）；桥接层 UUAVMqttBridgeComponent 的 OnFlighttaskProgress / OnReturnHomeStatus 直接拼平铺 data 后经 PublishEvent 发布；飞控层 UUAVFlightControlComponent 在 HandleFlighttaskPrepare 成功后返回 Success，在 OnDroneBatteryLow 中经 StartReturnHome 成功后广播 OnReturnHomeStatus(rth_auto_trigger, battery_low)；自动化测试通过 public Build* 入口断言 JSON 结构（UAVLiveCapacityTests 模式），UAVFlightControl 未暴露 current flight_id 的读取接口。

## Goals / Non-Goals

**Goals:**
- 对齐 dock 四类事件报文：flighttask_progress（result/output 包装）、return_home_info（替换 return_home_status）、flighttask_ready、hms。
- 飞控层新增任务就绪广播（flighttask_prepare 成功后触发），桥接层转发。
- 新增 public Build* 测试入口与自动化测试，覆盖四类报文结构。

**Non-Goals:**
- 不实现 /requests 通道（dock flighttask_prepare 已直接携带航线 URL，设备无需 flighttask_resource_get）。
- 不改动 OSD / state / services_reply 报文结构与 services 指令分发。
- 不做与协议无关的重构。

## Decisions

1. **事件常量集中定义**：在 UAV::CloudApi 新增 kEventReturnHomeInfo（return_home_info）、kEventFlighttaskReady（flighttask_ready）、kEventHms（hms）；kEventReturnHomeStatus 常量保留但不再被桥接层发布使用。
   替代方案：桥接层内联字符串 method——与现有常量体系不一致，弃用。

2. **flighttask_progress 报文结构**：data = { result: 0, output: { status, progress: { current_step, percent }, ext: { current_waypoint_index, media_count, flight_id, track_id, wayline_id, wayline_mission_state } } }，全部字段用 snake_case（dock Jackson 全局 SNAKE_CASE；现有 camelCase 字段全部改为 snake_case）。wayline_mission_state 映射：sent=5（到达首航点）、in_progress=6（执行中）、终态（ok/failed/canceled/timeout/partially_done）=9（结束）。
   替代方案：沿用平铺结构——dock getOutput() 为 null，状态机失效，弃用。

3. **return_home_info 报文**：桥接层 OnReturnHomeStatus 回调改为发布 kEventReturnHomeInfo；planned_path_points 由桥接层构造：返航路径为机场返航点（AirportOrigin + 返航高度），返航高度读取飞控组件新增的 GetCurrentRthAltitude()，last_point_type=0（有直飞返航点，无 DroneSim 时为空数组、65535）；flight_id 读取飞控组件新增的 GetCurrentFlightId()（flight_id 可能为空串，dock 允许）。
   替代方案：扩展 OnReturnHomeStatus 委托参数携带 flight_id——破坏现有 Blueprint 绑定，弃用。

4. **hms 报文**：OnMqttConnect 成功后发布 hms 空告警（list 空数组）；OnReturnHomeStatus(rth_auto_trigger/battery_low) 回调内连带发布 hms 低电量告警（code=fpv_tip_0x1B030014、device_type=0-100-1、imminent=true、in_the_sky=true、level=1、module=0、args.component_index=0）。hms 组装独立为 BuildHmsPayload 方法，测试可复用。

5. **测试入口**：桥接层新增 public Build* 方法：BuildFlighttaskProgressEventData、BuildReturnHomeInfoEventData、BuildFlighttaskReadyData、BuildHmsPayload，供 NewObject 自动化测试断言 JSON 结构与字段类型，沿用 BuildDockOsdPayload / BuildLiveCapacityPayload 的既有模式；测试文件新增 UAVProtocolEventsTests.cpp。

## Risks / Trade-offs

- [BREAKING 报文结构] dock 若缓存旧结构数据会解析失败 → 本次同时清理模拟器旧字段，且 dock 本身要求 output 非空，风险低。
- [wayline_mission_state 枚举映射] 若与 dock 版本枚举不一致会导致状态机误判 → 映射集中在一个辅助函数，联调时可单点调整。
- [flight_id 为空] 自动返航发生在任务外时 CurrentFlightId 为空 → dock 对 flight_id 空串容忍，且场景仅低电量自动返航，可控。

## Migration Plan

实现并补齐测试（Automation RunTests UAV）→ openspec validate --specs → 归档 → git commit + push。模拟器无持久化数据，无需数据迁移；若联调发现字段不符，回退为修改映射函数后重新发布。

## Open Questions

无。
