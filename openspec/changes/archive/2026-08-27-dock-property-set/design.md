## Context

桥接层（UUAVMqttBridgeComponent）已承担 services 分发、events 发布、OSD 组装与设备 state 发布。属性设置为纯状态模拟：属性值托管在桥接组件私有 USTRUCT 成员即可，无需新增模块。协议以 dock cloud-sdk 的 PropertySetEnum / TopicPropertySetRequest / PropertySetReplyResultEnum 与 sample 的 PropertySetFieldEnum / Receiver.valid 为口径（见 proposal.md - Why）。

## Goals / Non-Goals

**Goals:**
- 订阅 thing/product/{sn}/property/set 并回发 property/set_reply（result 0/1）。
- 支持 7 个无人机属性 + 1 个机场属性（dock-be PropertySetFieldEnum 实际启用的集合 + OSD 已有字段 exit_wayline_when_rc_lost），按后端 Receiver 口径校验。
- 无人机/机场 OSD 的对应字段改为读取属性状态（替代硬编码）。
- 新增自动化测试覆盖订阅解析、校验、回执与 OSD 联动。

**Non-Goals:**
- 不实现 property/get（dock cloud-sdk 无 get 通道，属性通过 OSD/state 上报）。
- 不实现 silent_mode / rth_mode / commander_mode_lost_action / commander_flight_height / offline_map_enable / thermal_* 等属性（dock-be 未启用、模拟器 OSD/state 无对应字段；后续变更按 DockStateDataKeyEnum 状态通道另行覆盖）。
- 不做属性持久化，重启后恢复默认值。
- 属性设置不联动飞控行为（如 height_limit 不钳制飞行高度），模拟范围聚焦协议与状态回显。

## Decisions

1. **属性状态托管在桥接组件**：新增 FUAVDroneProperties / FUAVDockProperties 两个 USTRUCT（BlueprintReadWrite 成员），与 OSD 构建/分发同层，默认值对齐当前 OSD 硬编码。备选：新增 UAVPropertyModule——纯状态模拟、无 Tick 逻辑，新增模块成本高于收益（与 OTA/远程调试同模式）。
2. **单属性处理**：物模型 property/set 一次设置一个属性，data 取第一个属性键处理；未识别属性回 result=1（对齐 PropertySetReplyResultEnum.FAILED）。
3. **校验口径对齐后端 Receiver**：night_lights_state/obstacle_avoidance/distance_limit_status.state 用 0/1 开关；height_limit 20-1500；rth_altitude 20-500；distance_limit 15-8000（缺省项按至少一项存在的规则）；rc_lost_action 0/1/2；exit_wayline_when_rc_lost 0/1；user_experience_improvement 0/1/2。
4. **回执复用 MakeMessageHeader 结构**：property/set_reply 报文 {tid（回填请求 tid）, bid, timestamp, data:{result}}，不带 method 字段（物模型标准，对齐 TopicPropertySetResponse）；发布入口 PublishPropertySetReply 独立实现。
5. **OSD 联动替换硬编码**：BuildDroneOsdPayload / BuildDockOsdPayload 改为读取属性状态；night_lights_state 不再跟随录像状态（原 IsRecording 推导为占位逻辑，语义错误，改为独立属性，默认 0）。
6. **订阅与连接流程复用**：OnMqttConnect 中与 services/drc 并列新增 property/set 订阅；OnPropertySetMessage 回调解析并提取 topic SN（thing/product/{sn}/property/set），分发至 DispatchPropertySetMessage。

## Risks / Trade-offs

- night_lights_state 语义修正（去掉 IsRecording 推导）→ 原 OSD 行为变化：录像不再点亮夜航灯字段；默认值 0 与后端枚举一致，OSD 测试同步更新。
- 属性校验在模拟器内轻量实现 → 与后端 Receiver.valid 口径一致但非同一实现；越界行为（回 result=1）与真实设备一致，dock-fe 不感知差异。
- 未知属性回 result=1 → 后续新增属性（silent_mode 等）只需扩展 HandlePropertySet 分支，不影响既有协议。
