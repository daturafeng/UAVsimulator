# camera-streaming Specification

## Purpose

UAVCameraStream 除直播指令外，处理载荷指令（载荷权/相机/云台），驱动 UAVDroneSim 载荷状态。

## ADDED Requirements

### Requirement: 载荷权指令
UAVCameraStream MUST 处理 payload_authority_grab：抢占载荷权（UAVDroneSim.SetPayloadAuthority(true)）并返回成功；载荷指令（camera_*/gimbal_*）执行前校验载荷权，未抢占返回 NoAuthority（result=2）。

#### Scenario: 抢占载荷权
- **WHEN** 收到 payload_authority_grab
- **THEN** 返回成功且 UAVDroneSim.HasPayloadAuthority 为 true

#### Scenario: 未抢权执行载荷指令
- **WHEN** 未收到 payload_authority_grab 而收到 camera_photo_take
- **THEN** 返回 NoAuthority 且载荷状态不变

### Requirement: 相机指令
UAVCameraStream MUST 处理 camera_mode_switch（0 拍照 / 1 录像，调用 UAVDroneSim.SetCameraMode）、camera_photo_take / camera_photo_stop（调用 TakePhoto / SetPhotoTaking(false)）、camera_recording_start / camera_recording_stop（调用 StartRecording / StopRecording），并广播 OnCommandResult。

#### Scenario: 模式切换
- **WHEN** 收到 camera_mode_switch 且 data.camera_mode=0
- **THEN** UAVDroneSim.GetCameraMode 返回 0，回 result=0

#### Scenario: 拍照指令
- **WHEN** 收到 camera_photo_take
- **THEN** UAVDroneSim 拍照状态推进，回 result=0

#### Scenario: 录像指令
- **WHEN** 收到 camera_recording_start
- **THEN** UAVDroneSim.IsRecording 返回 true（覆盖生效），回 result=0

### Requirement: 云台指令
UAVCameraStream MUST 处理 camera_aim（data 含 gimbal_pitch/gimbal_yaw，调用 UAVDroneSim.SetGimbalTarget）与 gimbal_reset（调用 ResetGimbalTarget），并广播 OnCommandResult。

#### Scenario: 云台瞄准
- **WHEN** 收到 camera_aim 且 data 含目标角
- **THEN** UAVDroneSim 云台目标被设置，回 result=0

#### Scenario: 云台复位
- **WHEN** 收到 gimbal_reset
- **THEN** UAVDroneSim 云台目标被清除，回 result=0
