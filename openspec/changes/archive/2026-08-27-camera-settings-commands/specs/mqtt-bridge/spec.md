# mqtt-bridge Specification

## Purpose

UAVMqttBridge 扩展载荷指令分发前缀，并将 OSD cameras 相机设置字段实时化。

## ADDED Requirements

### Requirement: 相机设置指令分发
UAVMqttBridge MUST 将 photo_storage_set / video_storage_set / ir_metering_mode_set / ir_metering_point_set / ir_metering_area_set / poi_mode_enter / poi_mode_exit / poi_circle_speed_set 等载荷指令分发到 UAVCameraStream.HandleCommand，并把 result 组装 services_reply 发布。

#### Scenario: 存储指令分发与回包
- **WHEN** 收到 photo_storage_set 的 services 报文
- **THEN** 调用 UAVCameraStream.HandleCommand 并在 services_reply 中返回其 result

#### Scenario: POI 指令分发与回包
- **WHEN** 收到 poi_mode_enter 的 services 报文
- **THEN** 调用 UAVCameraStream.HandleCommand 并在 services_reply 中返回其 result

### Requirement: OSD 相机设置字段
UAVMqttBridge MUST 在 OSD cameras 中输出实时相机设置：screen_split_enable 为 UAVDroneSim 分屏使能、photo_storage_settings / video_storage_settings 为存储位置数组、zoom_focus_mode / zoom_focus_value 为对焦模式/对焦值（zoom_focus_state 保持 0）。

#### Scenario: 分屏状态实时输出
- **WHEN** UAVDroneSim 分屏使能被设置为 true
- **THEN** OSD cameras.screen_split_enable 输出 true

