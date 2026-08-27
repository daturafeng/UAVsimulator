## 1. UAVDroneSim 载荷控制接口

- [x] 1.1 新增拍照状态：bPhotoTaking / RemainingPhotoNum / TakenPhotoCount，SetPhotoTaking / TakePhoto 接口，Tick 自动结束单张拍摄
- [x] 1.2 新增录像覆盖：bRecordingOverride 三态，StartRecording / StopRecording / ClearRecordingOverride，IsRecording 覆盖优先
- [x] 1.3 新增云台指令目标：GimbalTargetPitch / GimbalTargetYaw，SetGimbalTarget / ResetGimbalTarget，ComputeGimbalState 应用目标
- [x] 1.4 新增载荷权：bHasPayloadAuthority / SetPayloadAuthority / HasPayloadAuthority
- [x] 1.5 新增自动化测试：拍照计数与自动结束、录像覆盖优先级、云台目标叠加、载荷权查询

## 2. UAVCameraStream 载荷指令

- [x] 2.1 新增 SetDroneSim 注入与载荷指令处理：HandlePayloadAuthorityGrab / HandleCameraModeSwitch / HandleCameraPhotoTake / HandleCameraPhotoStop / HandleCameraRecordingStart / HandleCameraRecordingStop / HandleCameraAim / HandleGimbalReset
- [x] 2.2 HandleCommand 分发载荷 method 到对应处理函数，未抢载荷权返回 NoAuthority

## 3. UAVFlightControl 低电量自动返航

- [x] 3.1 抽出 StartReturnHome()（状态校验 + 返航航点），HandleReturnHome 复用
- [x] 3.2 BeginPlay 绑定 DroneSim->OnBatteryLow，OnDroneBatteryLow 校验后自动返航并广播 OnReturnHomeStatus

## 4. UAVMqttBridge 分发与报文

- [x] 4.1 UAVCloudApiTypes 增加载荷指令 method 常量（payload_authority_grab / camera_mode_switch / camera_photo_take / camera_photo_stop / camera_recording_start / camera_recording_stop / camera_aim / gimbal_reset）
- [x] 4.2 DispatchServicesMessage 分发 camera_*/payload_*/gimbal_* 到 CameraStream
- [x] 4.3 新增 PublishPayloadControlSource（对齐 dock report_control_source.py）；绑定 OnReturnHomeStatus 发布 return_home_status 事件

## 5. 验证与归档

- [x] 5.1 编译通过（UE 5.7 UBT 构建 UAVsimulatorEditor）
- [x] 5.2 自动化测试通过（UAV.Payload.* 新增项 + 全量 UAV.* 回归）
- [x] 5.3 openspec validate 校验变更并归档；git commit + push
