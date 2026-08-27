# drone-motion-simulation Specification

## Purpose

载荷状态（拍照/录像/云台/载荷权）可被指令控制，扩展被动模拟为可驱动模拟。

## ADDED Requirements

### Requirement: 拍照状态模拟
UUAVDroneSim MUST 维护拍照状态：bPhotoTaking（拍照中）与剩余照片数（初始 9999），提供 TakePhoto / SetPhotoTaking 接口；TakePhoto 使剩余照片数减 1（最低 0）、累计拍照数加 1，并开始拍照中状态；拍照中状态在启动后 3 秒由组件 Tick 自动结束。

#### Scenario: 拍照推进状态
- **WHEN** 调用 TakePhoto
- **THEN** 拍照中状态为真，剩余照片数减 1，累计拍照数加 1，3 秒后拍照中自动结束

#### Scenario: 剩余照片数下限
- **WHEN** 剩余照片数为 0 时再次 TakePhoto
- **THEN** 剩余照片数保持 0，累计拍照数仍加 1

### Requirement: 录像指令覆盖
UUAVDroneSim MUST 提供录像状态覆盖：StartRecording / StopRecording / ClearRecordingOverride；存在覆盖时 IsRecording 返回覆盖值，否则按飞行模式推导（起飞/航线/返航为录像中）。

#### Scenario: 指令覆盖推导
- **WHEN** 飞行状态为待机但调用 StartRecording
- **THEN** IsRecording 返回 true（指令优先）

#### Scenario: 清除覆盖恢复推导
- **WHEN** 调用 ClearRecordingOverride 且飞行状态为航线
- **THEN** IsRecording 返回 true（恢复按飞行模式推导）

### Requirement: 云台指令目标
UUAVDroneSim MUST 提供云台目标设置：SetGimbalTarget(Pitch, Yaw) 记录目标角并叠加到 UAVPayloadMath 计算结果（目标优先级高于时间微动），ResetGimbalTarget 清除目标恢复时间微动；输出角度保持归一化。

#### Scenario: 云台目标叠加
- **WHEN** 设置目标俯仰 -30 度后查询云台状态
- **THEN** 俯仰输出等于目标角（不再受时间微动影响）

#### Scenario: 云台目标复位
- **WHEN** 调用 ResetGimbalTarget
- **THEN** 云台输出恢复时间微动推导

### Requirement: 载荷权状态
UUAVDroneSim MUST 维护载荷权：SetPayloadAuthority / HasPayloadAuthority；载荷权初始未抢占，payload_authority_grab 成功后置为已抢占。

#### Scenario: 载荷权抢占
- **WHEN** 调用 SetPayloadAuthority(true)
- **THEN** HasPayloadAuthority 返回 true
