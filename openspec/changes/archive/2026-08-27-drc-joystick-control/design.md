## Context

参见 proposal.md - Why：DRC 链路（dock 前端虚拟座舱 → MQTT → 模拟器）缺失。现状：飞控层 HandleCommand 无 drc_* 分支；桥接层仅订阅 thing/product/{DockSn}/services 并在 services_reply 回执；无人机模拟组件只有航点/直接移动模型，无速度直驱接口；UAVCloudApiTypes 无 DRC topic/method 常量。

## Goals / Non-Goals

**Goals:**
- 实现 drc_mode_enter / drc_mode_exit（services 通道）与 drone_control / heart_beat / drone_emergency_stop（drc/down 通道）的解析、校验与执行。
- 新增 drc/up 回执通道与 drc_status_notify 事件上报链路。
- 无人机模拟组件新增摇杆速度控制（机体坐标 + 航向旋转 + 偏航角速度 + 指令过期悬停）。
- 自动化测试覆盖指令结果码、会话状态机、摇杆运动与事件/回执报文结构。

**Non-Goals:**
- 不实现真实 DRC MQTT broker 与设备注册（模拟器只维护会话状态标记）。
- 不实现心跳超时断链挂起摇杆（心跳仅正确回执；摇杆指令延迟由 delayTime 过期悬停兜底）。
- 不实现 joystick_invalid_notify 事件（无异常时无需上报）。
- 不改变既有 services 指令行为。

## Decisions

1. **DRC 双通道接入**：drc_mode_enter/exit 沿用 services 订阅与 services_reply 回执；drone_control/heart_beat/drone_emergency_stop 新增 drc/down 订阅与 drc/up 回执。理由：与 dock 协议（TopicDrcRequest/DrcUpData）一致。
   替代方案：全部并入 services 通道——与 dock topic 定义不符，弃用。

2. **回执报文复用 MakeServicesReply**：drc/up 报文 {tid, bid, timestamp, method, data:{result, output?}} 与 services_reply 结构一致，直接复用，仅在桥接层发布到 kTopicDrcUpTemplate topic。新增测试入口 BuildDrcUpReply（内部复用 MakeServicesReply）。
   替代方案：新增 MakeDrcUpReply 工具——重复实现相同结构，弃用。

3. **摇杆速度模型**：机体坐标（前=+x、右=+y、上=+h），按航向旋转得到场景速度（航向 0=北=+Y 场景轴，前向=(sin H, cos H, 0)，右向=(cos H, -sin H, 0)）；x/y 按 ±17 归一化到 MaxHorizontalSpeed，h 按 ±5 归一化到 MaxVerticalSpeed，w 直接作为偏航角速度（度/秒）更新朝向；速度用响应系数平滑（smooth = clamp(Δt × freq, 0, 1)）。
   替代方案：直接按最大速度瞬移——运动突兀且与 freq 语义不符，弃用。

4. **指令过期悬停**：记录最近摇杆指令时间与 delayTime，Tick 中超过 delayTime 未收到新指令时摇杆目标归零（悬停），会话保持；新指令立即恢复。
   替代方案：过期即退出会话——与真实设备断链行为不符（设备会悬停等待恢复），弃用。

5. **摇杆模式与任务互斥**：drone_control 成功时 DroneSim->StopMission() 停止航点任务；摇杆模式独立于任务标记（bJoystickActive），HasActiveMission 不受影响返回 false；Tick 中摇杆模式优先于任务推进。

6. **seq 回执**：飞控在 drone_control/heart_beat 中解析并保存 LastDrcSeq（缺失返回 InvalidParams），桥接层通过 GetLastDrcSeq() 组装 output={seq}；drone_emergency_stop 不携带 seq（对齐 dock DrcUpData 无 output）。

7. **services 通道精确匹配**：DispatchServicesMessage 仅对 kMethodDrcModeEnter / kMethodDrcModeExit 精确匹配分发到飞控；其他 drc_* 前缀方法（如 drc_initial_state_subscribe）保持现有未知指令路径（回非 0 services_reply）。

8. **事件上报**：飞控广播 OnDrcStatusNotify(int32 DrcState)，桥接层绑定后发布 kEventDrcStatusNotify，data={result:0, drc_state}，复用现有 PublishEvent 链路。

## Risks / Trade-offs

- [运动真实性] 摇杆速度按归一化比例折算，非真实飞控动力学；联调以位移/朝向变化趋势为准。
- [急停语义] drone_emergency_stop 仅停止运动并保持 DRC 会话；真实设备会保持连接，后续可扩展退出会话行为。
- [参数范围] dock 校验 x∈[-17,17]、y∈[-17,17]、h∈[-4,5]、w∈[-90,90]、freq∈[2,10]、delayTime∈[100,1000]，模拟器同样校验，越界回 InvalidParams。

## Migration Plan

实现并补齐测试（Automation RunTests UAV）→ openspec validate --specs → 归档 → git commit + push。模拟器无持久化数据，无需数据迁移；联调若发现回执字段不符，回退为修改 BuildDrcUpReply / 事件 data 后重新发布。

## Open Questions

无。
