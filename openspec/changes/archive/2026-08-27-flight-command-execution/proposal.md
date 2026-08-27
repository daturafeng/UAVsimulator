## Why

当前 UAVsimulator 仅有模块骨架：飞控状态机、无人机移动、相机推流都是占位实现，无法解析并执行 dock（大疆上云 API）下发的真实飞控指令（起飞到点、航线任务、返航），无人机也不会按指令受控移动。首期目标是 M4TD 无人机 + dock3 机场，需要先让"指令 → 状态机 → 运动模拟"这条主链路按协议跑通。

## What Changes

- UAVCore 新增上云 API 协议常量与工具：Topic 模板、method 枚举、默认设备标识（机场 DOCK3TEST001、无人机 1581F8HGXTEST001）、JSON 报文头构造。
- UAVDroneSim 引入经纬度/海拔 ↔ UE 场景坐标转换（以机场为原点的局部 ENU 坐标系），并实现速度可控的航点移动（水平速度、垂直速度、朝向跟随、到达判定）。
- UAVFlightControl 实现飞控指令协议解析与状态机：飞控权抢占（flight_authority_grab）、起飞到点（takeoff_to_point）、航线任务（flighttask_create/prepare/execute/undo/pause/recovery）、返航（return_home/return_home_cancel）、直播控制（live_start_push/live_stop_push/live_set_quality/live_lens_change）。
- UAVFlightControl 提供指令处理结果与事件回调（如 takeoff_to_point_progress、flighttask_progress），供 MQTT 桥接层转发 services_reply 与 events。
- UAVCameraStream 建立直播会话模型（video_id 规则、RTMP 地址生成、清晰度/镜头切换），推流管线在本变更中仅定义接口，实际编码推流在后续变更接入。

## Capabilities

### New Capabilities

- `flight-control-protocol`: 上云 API 飞控指令的解析、状态机与事件模型。
- `drone-motion-simulation`: 无人机受控运动模拟，含地理坐标转换与航点移动。

### Modified Capabilities

无。

## Impact

- `Source/UAVCore`：新增协议常量与工具类。
- `Source/UAVDroneSim`：扩展运动模拟组件与坐标工具。
- `Source/UAVFlightControl`：替换占位实现为完整指令状态机。
- `Source/UAVCameraStream`：新增直播会话模型。
- 依赖关系保持 `UAVFlightControl → UAVDroneSim → UAVCore`、`UAVCameraStream → UAVCore` 不变。
- 不引入第三方依赖；MQTT 客户端接入（MQTTCore 插件）留待下一变更。
