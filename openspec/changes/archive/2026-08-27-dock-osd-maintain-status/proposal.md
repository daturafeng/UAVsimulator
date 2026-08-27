## Why

上一变更 dock-osd-alignment 后，机场 OSD（thing/product/{DockSn}/osd）已补齐 dock OsdDock 的绝大多数字段，但遗漏了 OsdDock 的 maintain_status（OsdDockMaintainStatus）字段：规格与测试均未覆盖，BuildDockOsdPayload 输出缺少该字段，dock 后端 Jackson 反序列化 OsdDock 时 maintainStatus 为 null。

dock 侧口径（OsdDock.java + OsdDockMaintainStatus.java + DockMaintainStatus.java）：maintain_status 为对象，含 maintain_status_array 数组，数组元素为 { last_maintain_flight_sorties, last_maintain_time, last_maintain_type, state }；JSON 采用 snake_case（dock-be 全局 Jackson SNAKE_CASE 策略）。维护状态无真实物理模型，按新机场基线输出（未保养、未到期）。

## What Changes

- UAVMqttBridge 机场 OSD 组装补齐 maintain_status 字段：输出 maintain_status_array 数组（一个 DockMaintainStatus 元素：last_maintain_flight_sorties=0、last_maintain_time=0、last_maintain_type=0（NO）、state=false），字段名与类型对齐 OsdDockMaintainStatus。
- UAVMqttBridge 自动化测试补全：机场 OSD 结构测试增加 maintain_status 顶层字段与子对象结构断言。

## Capabilities

### New Capabilities
<!-- 无：能力建立在已有 mqtt-bridge 之上 -->

### Modified Capabilities
- mqtt-bridge: 机场 OSD 补齐 maintain_status（维护状态）字段，字段集完整对齐 dock OsdDock。

## Impact

- Source/UAVMqttBridge：UAVMqttBridgeComponent.BuildDockOsdPayload 增加 maintain_status 子对象组装。
- Source/UAVMqttBridge/Private/Tests：UAVDockOsdTests 结构测试补充 maintain_status 字段与 maintain_status_array 结构断言。
- 行为变化：dock 解析机场 OSD 后 maintainStatus 不再为空；无协议与 Topic 变化。
