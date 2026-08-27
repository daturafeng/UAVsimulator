## Why

模拟器已支持 services 通道的飞控/相机指令，但 dock 的 DRC（虚拟摇杆直控）链路仍未实现：drc_mode_enter / drone_control / heart_beat / drone_emergency_stop / drc_mode_exit 统一回 UnknownMethod，drc_status_notify 事件缺失。dock-fe 虚拟座舱（useDrcSession.ts）与 dock 后端 DrcServiceImpl 已完整支持 DRC 通道（thing/product/{sn}/drc/down|up），联调时无法用虚拟座舱直控无人机。

## What Changes

- UAVCore：新增 DRC topic 模板 kTopicDrcDownTemplate / kTopicDrcUpTemplate，指令 method 常量 drc_mode_enter / drc_mode_exit / drone_control / heart_beat / drone_emergency_stop，事件 method 常量 drc_status_notify。
- UAVDroneSim：新增摇杆速度控制 SetJoystickCommand(x,y,h,w,freq,delayTime)：按机体坐标系（前/右/垂直）折算速度并旋转到航向，w 为偏航角速度，速度平滑过渡，delayTime 内无新指令自动悬停；摇杆模式停止现有航点任务且不计入任务。
- UAVFlightControl：新增 DRC 会话状态机：drc_mode_enter（校验飞控权与空中状态）、drc_mode_exit、drone_control（参数范围校验 + 停止现有任务 + 驱动摇杆）、heart_beat（回执）、drone_emergency_stop（停止一切运动）；新增 OnDrcStatusNotify 委托（进入 CONNECTED=2 / 退出 DISCONNECTED=0）。
- UAVMqttBridge：订阅 thing/product/{DockSn}/drc/down，分发 drone_control / heart_beat / drone_emergency_stop 到飞控并回发 thing/product/{sn}/drc/up（data:{result, output:{seq}}，急停无 output）；services 通道精确分发 drc_mode_enter / drc_mode_exit；绑定 OnDrcStatusNotify 发布 drc_status_notify 事件。

## Capabilities

### New Capabilities
<!-- 无：能力建立在已有 mqtt-bridge、flight-control-protocol 与 drone-motion-simulation 之上 -->

### Modified Capabilities
- flight-control-protocol: 新增 DRC 摇杆直控会话状态机与指令处理（enter/exit/control/heart_beat/emergency_stop）及 drc_status_notify 状态事件。
- mqtt-bridge: 新增 DRC 指令通道（drc/down 订阅、drc/up 回执）与 drc_status_notify 事件转发。
- drone-motion-simulation: 新增摇杆速度控制模型（机体坐标速度、偏航角速度、指令过期悬停）。

## Impact

- Source/UAVCore：UAVCloudApiTypes.h/.cpp 新增 topic 模板与 method/event 常量。
- Source/UAVDroneSim：UAVDroneSimComponent 新增 SetJoystickCommand / SetJoystickActive 与 Tick 摇杆物理。
- Source/UAVFlightControl：UAVFlightControlComponent 新增 DRC 会话成员、五个指令处理器与 OnDrcStatusNotify 委托。
- Source/UAVMqttBridge：UAVMqttBridgeComponent 新增 drc/down 订阅、DispatchDrcMessage / PublishDrcUpReply、OnDrcStatusNotify 回调与 BuildDrcUpReply / BuildDrcStatusNotifyData 测试入口。
- Source/UAVMqttBridge/Private/Tests 与 Source/UAVDroneSim/Private/Tests：新增 DRC 摇杆/事件自动化测试。
- 行为变化：dock-fe 虚拟座舱可进入 DRC 直控，摇杆指令实时驱动无人机运动，急停停止运动，DRC 状态通过 drc_status_notify 事件上报。
