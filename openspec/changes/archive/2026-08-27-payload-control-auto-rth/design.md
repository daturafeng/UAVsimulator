## Context

上一变更 payload-state-simulation 完成后，UAVDroneSim 已有载荷状态（BatteryCapacityPercent、GimbalState、CameraMode、ZoomFactor、IsRecording 按飞行模式推导、OnBatteryLow 事件），OSD 组装已对齐 dock。当前缺口：载荷指令（payload_authority_grab / camera_* / gimbal_*）在 UAVMqttBridge::DispatchServicesMessage 中既不匹配飞行前缀也不匹配 live_ 前缀，统一走 UnknownMethod；低电量事件无人消费。

dock 侧口径（simulate_control_services.py / report_control_source.py）：
- payload_authority_grab：回 result=0，0.2 秒后向 thing/product/{无人机SN}/state 发布载荷控制源报文 {control_source:"A", payloads:[{control_source,payload_index}]}。
- camera_mode_switch / camera_photo_take / camera_photo_stop / camera_recording_start / camera_recording_stop / camera_aim / gimbal_reset 等均在"支持自动成功的方法"集合内，回 result=0。
- OSD 消费的载荷字段：photo_state（0/1）、remain_photo_num、recording_state（0/1）、zoom_factor、gimbal_*。

## Goals / Non-Goals

**Goals:**

- 载荷权抢占：payload_authority_grab 回 0，校验后续载荷指令，抢权后上报载荷控制源 state。
- 相机指令：camera_mode_switch 切换拍照/录像模式；camera_photo_take/stop 推进拍照状态与剩余照片数；camera_recording_start/stop 覆盖录像状态（指令优先）。
- 云台指令：camera_aim 设置目标俯仰/偏航（按指令叠加到基础角）；gimbal_reset 复位云台角。
- 低电量自动返航：电量低于返航阈值且在空中时自动返航，返回中不重复触发。
- 载荷指令分发与载荷权校验可测（纯逻辑自动化测试）。

**Non-Goals:**

- 不做拍照图片内容生成（仅状态与计数，照片流属后续能力）。
- 不做 camera_look_at / camera_screen_split / camera_exposure_* / camera_focus_* 等高级相机指令（保持 UnknownMethod 非 0，dock 不依赖）。
- 不做自动降落（低电量返航到点后仍走 Landing → Idle 既有流程）。

## Decisions

- **载荷指令归属 UAVCameraStream**：该组件已处理 live_* 载荷指令、已有 result 码语义与 OnCommandResult 事件，且 Build.cs 已依赖 UAVDroneSim；新增 SetDroneSim 注入与载荷指令处理函数，避免新建模块。
- **拍照状态模型**：UAVDroneSim 维护 bPhotoTaking（拍照中）与 RemainingPhotoNum（默认 9999，每次 photo_take 减 1，最低 0）、TakenPhotoCount（累计）；photo_state OSD 输出 bPhotoTaking ? 1 : 0。photo_take 成功后 3 秒自动结束拍照（模拟单张拍摄），由组件 Tick 推进。
- **录像覆盖模型**：UAVDroneSim 新增 bRecordingOverride（三态：未设置 / true / false），IsRecording() 改为"覆盖优先，否则按飞行模式推导"；camera_recording_start 置 true、stop 置 false；recording override 也影响 recording_state OSD。
- **云台指令模型**：camera_aim 的 data 含 gimbal_pitch/gimbal_yaw（相对当前机头），设置 GimbalTargetPitch/Yaw 并叠加到 UAVPayloadMath 计算结果（Target 优先级高于时间微动，保持归一化）；gimbal_reset 清空 Target。
- **载荷权模型**：UAVDroneSim 维护 bHasPayloadAuthority（payload_authority_grab 置 true），载荷指令（camera_*/gimbal_*）执行前校验，未抢权返回 NoAuthority（对齐飞控权语义，result=2）。
- **低电量自动返航复用 HandleReturnHome 路径**：UAVFlightControl 抽出 StartReturnHome()（含状态校验与航点构建），HandleReturnHome 与 OnDroneBatteryLow 共用；OnDroneBatteryLow 仅在持有飞控权、非 Idle/Landing/ReturnHome 时触发，触发后由 bReturnHomePending 防抖（返航中不重复触发）。
- **自动返航事件**：UAVFlightControl 广播 OnReturnHomeStatus（status=rth_auto_trigger，reason=battery_low），UAVMqttBridge 绑定并发布 events return_home_status，data 含 result=0/status/reason。
- **载荷控制源 state 报文**：UAVMqttBridge 新增 PublishPayloadControlSource，复用 MakeTelemetryHeader，data {control_source:"A", payloads:[{control_source:"A",payload_index:"52-0-0"}]}，在 payload_authority_grab 回包后立即发布（对齐 dock 0.2s 语义，无需延时）。

## Risks / Trade-offs

- [载荷指令无真实相机硬件] → 拍照/录像只模拟状态与计数，OSD 与 events 表现一致即可，图片/视频流属后续能力。
- [自动返航打断人工任务] → 与真实无人机低电量行为一致；返航前停止当前任务，dock 通过 OSD mode_code=9 感知。
- [云台指令与时间微动叠加] → 指令目标优先，微动仅在无指令目标时生效；dock 为演示级消费，精度满足联调。

## Open Questions

无。
