## Context

UAVFlightControl / UAVDroneSim / UAVCameraStream 已实现（见已归档变更 flight-command-execution），事件通过委托暴露（OnCommandResult / OnTakeoffProgress / OnFlighttaskProgress / OnLiveStatusChanged）。dock 侧协议口径：topic 模板 thing/product/{sn}/services(_reply)/events/osd/state、sys/product/{sn}/status；报文头 {tid,bid,timestamp,method,data}，事件另加 gateway；broker 默认 10.100.51.15:1883（root/unis@123）。引擎自带 MQTT 插件预编译二进制与源码不同步且头文件缺少 API 导出宏（LNK2019），已将源码（Engine/Plugins/Protocols/MQTT 的 MQTTCore）内置为项目插件 UAVMQTT（模块 UAVMQTTCore，补齐 UAVMQTTCORE_API 导出宏），在 .uproject 启用，引擎自带 MQTT 插件停用。

## Goals / Non-Goals

**Goals:**

- 与 dock 打通 MQTT：收 services、回 services_reply、发 events、周期 OSD/state/status。
- 桥接层保持可测：指令分发与报文组装不依赖真实 broker。

**Non-Goals:**

- 不做真实 RTMP 推流（仍由 UAVCameraStream 会话模型占位，推流管线后续变更）。
- 不做 DRC 双向通道（drc/down、drc/up）与 requests/requests_reply 双向流（预留 topic 但不实现）。
- 不做云台/拍照/录像指令（camera_*、gimbal_*）的业务实现，仅未知指令统一回非 0。

## Decisions

- **内置引擎 MQTT 源码为项目插件 UAVMQTT**：满足"既有能力优先"，避免自研 MQTT 协议栈；因引擎预编译二进制与源码不同步（Connect/Disconnect/Publish/Subscribe/SetOnMessageHandler 缺符号），内置源码副本并补齐导出宏。通过 UMQTTSubsystem::GetOrCreateClient(InParent, FMQTTURL) 获取客户端，UMQTTClientObject 提供 Connect/Subscribe/Publish，UMQTTSubscriptionObject::SetOnMessageHandler 接收消息。
- **模拟器扮演机场角色**：订阅 thing/product/{机场SN}/services（dock 下发指令的入口），回发 services_reply；events/osd/state/status 均为上行发布。OSD 区分无人机与机场两个 topic（thing/product/{无人机SN}/osd 与 thing/product/{机场SN}/osd），首期只实现无人机 OSD 完整结构，机场 OSD 复用精简字段。
- **指令分发与报文组装独立成纯函数**：新增 UAVCloudApiTypes 的报文工具（构造 services_reply/events/state 报文）与 UAVMqttBridge 内静态解析函数（services 报文 → method+dataJson），便于单元测试。
- **配置默认值与 dock 对齐**：broker 10.100.51.15:1883、root/unis@123、DOCK3TEST001、1581F8HGXTEST001、52-0-0；所有字段 EditAnywhere 可覆盖。
- **OSD 遥测周期 1 秒**：与 dock 假设备上报频率一致；通过组件 Tick 累计计时器实现，不依赖定时器子系统。
- **事件转发用委托订阅**：BeginPlay 时绑定飞控/相机委托，回调内只做报文组装与发布，保证线程安全（MQTT 回调在游戏线程）。

## Risks / Trade-offs

- [引擎 MQTT 预编译二进制与源码不同步、缺导出符号] → 已内置源码副本为 UAVMQTT 插件并补齐 UAVMQTTCORE_API 导出宏；仅使用其稳定公开 API，封装在 UAVMqttBridge 内部，便于未来替换。
- [broker 地址为内网环境] → 全部可配置，默认值仅便于本地联调；文档注明。
- [OSD 字段众多，逐一模拟成本高] → 首期实现 dock 联调必需的核心字段（位置/高度/朝向/速度/模式编码/电量/云台/摄像头），其余字段给合理默认值。

## Open Questions

无。
