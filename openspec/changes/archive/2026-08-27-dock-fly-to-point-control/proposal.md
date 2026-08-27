## Why

模拟器已支持 takeoff_to_point / flighttask_* / return_home_* 等飞控指令，但 dock 的 ControlMethodEnum / simulate_control_services.py 明确支持的“指点飞行”（fly_to_point）三件套仍未实现：fly_to_point（飞向目标点）、fly_to_point_stop（停止指点飞行）、fly_to_point_update（更新目标点与速度）统一回 UnknownMethod，联调时 dock 下发的指点飞行指令全部失败，dock 端控制台也无法收到 fly_to_point_progress 进度事件。

## What Changes

- UAVCore 新增服务指令 method 常量：fly_to_point / fly_to_point_stop / fly_to_point_update，以及事件 method 常量 fly_to_point_progress。
- UAVFlightControl 新增指点飞行处理：HandleFlyToPoint（校验飞控权与参数，中断当前任务，飞向 points[0]，广播 wayline_progress）、HandleFlyToPointStop（停止指点飞行，广播 wayline_cancel）、HandleFlyToPointUpdate（复用当前会话更新目标点与速度，重新飞向新目标）；新增 OnFlyToPointProgress 委托，航点到达/完成时广播 wayline_progress / wayline_ok，复用空中巡航（Flying）状态。
- UAVMqttBridge 指令分发补充 fly_ 前缀（当前仅 flight_/takeoff_/return_home 前缀，fly_to_point 会落空）；绑定 OnFlyToPointProgress 发布 fly_to_point_progress 事件，data 对齐 dock FlyToPointProgress：{ result, status, fly_to_id, way_point_index }。

## Capabilities

### New Capabilities
<!-- 无：能力建立在已有 mqtt-bridge 与 flight-control-protocol 之上 -->

### Modified Capabilities
- mqtt-bridge: services 指令分发新增 fly_to_point / fly_to_point_stop / fly_to_point_update，事件转发新增 fly_to_point_progress。
- flight-control-protocol: 新增指点飞行指令状态机（解析、中断当前任务、进度事件）。

## Impact

- Source/UAVCore：UAVCloudApiTypes.h/.cpp 新增指令/事件 method 常量。
- Source/UAVFlightControl：UAVFlightControlComponent 新增 HandleFlyToPoint / HandleFlyToPointStop / HandleFlyToPointUpdate 与 OnFlyToPointProgress 委托。
- Source/UAVMqttBridge：DispatchServicesMessage 分发前缀补充 fly_，新增 OnFlyToPointProgress 回调与 BuildFlyToPointProgressEventData 测试入口。
- Source/UAVMqttBridge/Private/Tests：新增指点飞行报文与状态机自动化测试。
- 行为变化：dock 下发指点飞行指令后模拟器回 0 并驱动无人机飞向目标点，停止/更新指令生效，进度通过 fly_to_point_progress 事件上报。
