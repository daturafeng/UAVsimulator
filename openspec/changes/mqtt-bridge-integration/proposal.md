## Why

上一变更（flight-command-execution）已完成"指令 → 状态机 → 运动模拟"主链路，但 UAVMqttBridge 仍是占位实现（Connect 只做字符串判空），无法与 dock（D:\WebCode\dock）真实联调：dock 下发的 services 指令收不到、services_reply/events 事件回不去、OSD 遥测也没有上报。要让模拟器真正扮演"机场 + 无人机"接入 dock，必须打通 MQTT 通道。

## What Changes

- 启用 UE 5.7 引擎自带 MQTT 插件（Engine/Plugins/Protocols/MQTT 的 MQTTCore），在 .uproject 登记启用，UAVMqttBridge 增加 MQTTCore 模块依赖。
- UAVMqttBridge 组件重写为完整桥接：
  - 连接管理：通过 UMQTTSubsystem 创建客户端，连接/断开、连接状态回调，配置默认值对齐 dock（broker 10.100.51.15:1883、root/unis@123，可编辑覆盖）。
  - 指令下发订阅：订阅 thing/product/{机场SN}/services，解析 services 报文（tid/bid/method/data），按 method 分发到 UAVFlightControl / UAVCameraStream，并回发 thing/product/{机场SN}/services_reply（result 0/非0）。
  - 事件转发：订阅飞控进度委托（OnTakeoffProgress / OnFlighttaskProgress / OnCommandResult）与相机直播状态，拼装 thing/product/{机场SN}/events 报文（含 gateway 字段，与 dock 口径一致）。
  - OSD 遥测：周期（默认 1 秒）从 UAVDroneSim 读取位置/朝向/速度，组装无人机 OSD 报文发布到 thing/product/{无人机SN}/osd，结构对齐 dock report_drone_osd.py；同步发布机场 OSD（thing/product/{机场SN}/osd）与设备 state（thing/product/{sn}/state）、在线状态（sys/product/{机场SN}/status）。
- 接入方式保持可测：指令分发与报文组装为纯函数式接口，可脱离 broker 单独单元测试。

## Capabilities

### New Capabilities

- `mqtt-bridge`: 上云 API MQTT 桥接：连接管理、指令订阅与分发、服务回复、事件转发、OSD/状态周期上报。

### Modified Capabilities

- `flight-control-protocol`: 无协议改动，桥接层消费其事件委托。

## Impact

- `UAVsimulator.uproject`：新增 MQTT 插件启用。
- `Source/UAVMqttBridge`：替换占位实现为完整 MQTT 桥接组件。
- 依赖关系：UAVMqttBridge 新增 MQTTCore 依赖；保持 UAVMqttBridge → UAVFlightControl/UAVDroneSim/UAVCameraStream/UAVCore。
- 不引入第三方依赖（MQTTCore 为引擎自带插件）。
