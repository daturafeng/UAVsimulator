# 设计：直播能力 state 上报

## Context

桥接层现有 state 发布模式：PublishDeviceState / PublishPayloadControlSource 均用 MakeTelemetryHeader + kTopicStateTemplate 发布 thing/product/{sn}/state（无 method，data 携带业务键）。dock 的 StateRouter 按 gateway 设备类型 + data 键集合路由：DOCK/DOCK2/DOCK3 走 DockStateDataKeyEnum，键 live_capacity 映射 DockLivestreamAbilityUpdate，进而触发 SDKLivestreamService.dockLivestreamAbilityUpdate。因此只需复用现有 state 发布通道，data 组装为 { live_capacity: {...} } 即可，无需新增订阅或 method。

报文数据源全部为静态能力声明（无真实物理模型），对齐 dock 联调脚本 report_live_capacity.py：网关设备（165-0-7 相机，normal-0/normal）与无人机设备（176-0-0 普通相机 + 52-0-0 主载荷，normal-0/zoom，可切换 normal/wide/zoom/ir）。

## Goals / Non-Goals

**Goals:**

- 连接成功后发布一次直播能力 state，dock 端能力缓存建立。
- 报文字段名与结构对齐 DockLiveCapacity / DockLiveCapacityDevice / DockLiveCapacityCamera / DockLiveCapacityVideo（snake_case）。
- 自动化测试覆盖报文结构。

**Non-Goals:**

- 不实现 rc_livestream_ability_update（遥控器能力，模拟器无 RC 设备，跳过）。
- 不做能力动态变更上报（无真实物理模型，连接时按基线声明一次）。
- 不改协议与 Topic、不新增配置项（相机索引/SN 复用现有配置）。

## Decisions

- **组装入口**：新增 BuildLiveCapacityPayload() const（返回 data 对象）与 PublishLiveCapacity()（发布 state），与 BuildDockOsdPayload / PublishDockOsd 模式一致，便于测试直接调用组装函数。
- **结构**：data = { live_capacity: { available_video_number: 3, coexist_video_number_max: 3, device_list: [网关项, 无人机项] } }。网关项 available_video_number=1、coexist=1；无人机项 available_video_number=2、coexist=1；总数为 3（对齐脚本 sum 逻辑）。
- **挂载点**：OnMqttConnect 中 PublishDeviceState 之后调用 PublishLiveCapacity()（与 PublishPayloadControlSource 同层级，非周期上报）。
- **测试**：UAVLiveCapacityTests.cpp 用 NewObject + SetDroneSim 范式（沿用 UAVDockOsdTests），断言顶层 live_capacity、device_list 两设备项、camera/video 字段与值（165-0-7、176-0-0、52-0-0、zoom、可切换类型数组）。

## Risks / Trade-offs

- 仅连接时上报一次：若 dock 在模拟器连接后才订阅 state，可能错过首包；但模拟器周期 OSD 与状态保持在线，dock 重连场景由联调环境既有机制兜底，且与脚本 report_live_capacity.py 单次发布口径一致。
- 视频流总数硬编码 3：若未来新增相机需同步调整，当前首期单相机载荷场景下与脚本输出一致。
