# mqtt-bridge Specification

## Purpose

载荷指令分发到 UAVCameraStream；载荷权抢占后上报控制源 state；自动返航事件转发。

## ADDED Requirements

### Requirement: 载荷指令分发
UAVMqttBridge MUST 将 services 报文中 payload_authority_grab / camera_mode_switch / camera_photo_take / camera_photo_stop / camera_recording_start / camera_recording_stop / camera_aim / gimbal_reset 分发到 UAVCameraStream.HandleCommand，并把 result 组装 services_reply 发布。

#### Scenario: 载荷指令分发与回包
- **WHEN** 收到 camera_recording_start 的 services 报文
- **THEN** 调用 UAVCameraStream.HandleCommand 并在 services_reply 中返回其 result

### Requirement: 载荷控制源上报
UAVMqttBridge MUST 在 payload_authority_grab 成功后向 thing/product/{无人机SN}/state 发布载荷控制源报文：data {control_source:"A", payloads:[{control_source:"A", payload_index:"52-0-0"}]}（对齐 dock report_control_source.py）。

#### Scenario: 抢权后上报控制源
- **WHEN** payload_authority_grab 返回成功
- **THEN** 发布 thing/product/{无人机SN}/state 载荷控制源报文

### Requirement: 自动返航事件
UAVMqttBridge MUST 绑定 UAVFlightControl.OnReturnHomeStatus，将 rth_auto_trigger 事件拼装为 thing/product/{机场SN}/events 报文（method=return_home_status，data 含 result/status/reason）发布。

#### Scenario: 自动返航事件转发
- **WHEN** 飞控广播 OnReturnHomeStatus（rth_auto_trigger/battery_low）
- **THEN** 发布 events 报文 return_home_status，data 含 result=0/status=rth_auto_trigger/reason=battery_low
