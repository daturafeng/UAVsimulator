# camera-streaming Specification

## Purpose
相机载荷的直播会话与真实 RTMP 推流能力：将无人机相机画面编码为 H.264 并通过 RTMP 推送给 dock 的流媒体服务器，支持清晰度档位切换与推流生命周期管理，供上云 API 直播指令（live_start_push / live_stop_push / live_set_quality / live_lens_change）驱动。
## Requirements
### Requirement: 直播会话模型
UAVCameraStream MUST 维护直播会话（video_id、RTMP 地址、清晰度、镜头类型、推流状态），并按上云 API 直播指令创建/更新/结束会话。

#### Scenario: 会话创建
- **WHEN** 收到 live_start_push（url_type=1 且携带 url 或 video_id）
- **THEN** 创建或复用对应 video_id 的会话，标记推流中，并广播 live_status 事件

#### Scenario: 会话结束
- **WHEN** 收到 live_stop_push（video_id 已存在）
- **THEN** 会话标记为停止推流，并广播 live_status 事件

#### Scenario: 清晰度切换
- **WHEN** 收到 live_set_quality（video_quality 在 0-4 范围内且会话存在）
- **THEN** 会话清晰度更新并广播 live_status 事件；非法档位或会话不存在时返回非 0

#### Scenario: 镜头切换
- **WHEN** 收到 live_lens_change（video_type 为 normal/thermal/wide/zoom 且会话存在）
- **THEN** 会话镜头类型与 video_id 更新并广播 live_status 事件

### Requirement: 真实 RTMP 推流
UAVCameraStream MUST 在会话处于推流状态时，将画面捕获源（RenderTarget）的像素按会话清晰度档位编码为 H.264，并以 FLV 格式推送到会话的 RTMP 地址。

#### Scenario: 推流启动
- **WHEN** 会话标记为推流中且 FFmpeg 可用
- **THEN** 启动 FFmpeg 编码推流子进程，按档位分辨率/码率/帧率持续推送画面

#### Scenario: 推流停止
- **WHEN** 会话标记为停止推流
- **THEN** 停止编码推流并回收 FFmpeg 子进程资源

#### Scenario: 推流兜底清理
- **WHEN** 组件销毁或关卡结束而会话仍在推流
- **THEN** 停止推流并回收子进程，不产生悬挂进程

### Requirement: FFmpeg 可用性降级
UAVCameraStream MUST 在 FFmpeg 不可用或严格模式未满足时保持可用的降级行为，不崩溃，且推流状态可感知。

#### Scenario: FFmpeg 缺失降级
- **WHEN** FFmpeg 不可用且未开启严格模式
- **THEN** 会话模型与事件保持原有行为（推流标记为占位），并记录警告日志

#### Scenario: 严格模式失败
- **WHEN** 开启严格模式且 FFmpeg 不可用
- **THEN** live_start_push 返回非 0，不创建推流

### Requirement: 清晰度档位映射
UAVCameraStream MUST 将 video_quality（0-4）映射为推流参数：0 自适应 / 1 流畅 / 2 标清 / 3 高清 / 4 超清，自适应档位按可用资源选择合理默认参数。

#### Scenario: 档位参数可用
- **WHEN** 查询任意 0-4 档位的推流参数（分辨率/码率/帧率）
- **THEN** 返回对应档位的确定参数，档位映射可脱离 FFmpeg 单独校验

### Requirement: 推流失败自动重连
UAVCameraStream MUST 在 FFmpeg 推流子进程意外退出时按可配置策略自动重新启动推流：重连成功则恢复推流；达到重连次数上限后才停止会话并广播状态事件。重连策略（次数上限、基础间隔、退避上限）可配置，策略计算可脱离 FFmpeg 单独校验。

#### Scenario: 意外退出触发重连
- **WHEN** FFmpeg 推流子进程意外退出（如 RTMP 服务器不可达）且重连未耗尽
- **THEN** 组件回收子进程与管道，按退避间隔重新启动 FFmpeg 推流，重连期间会话保持推流状态

#### Scenario: 重连耗尽停止
- **WHEN** FFmpeg 重连次数达到上限仍无法建立推流
- **THEN** 会话标记为停止推流，广播 live_status 事件，并回收子进程资源

#### Scenario: 重连可取消
- **WHEN** 重连等待期间收到 live_stop_push 或组件销毁
- **THEN** 取消重连调度并停止会话，不产生悬挂定时器或悬挂进程

#### Scenario: 重连策略可配置
- **WHEN** 重连次数上限为 0
- **THEN** 不启用自动重连，FFmpeg 意外退出时按原有行为立即停止会话

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

### Requirement: 相机设置指令
UAVCameraStream MUST 处理相机设置指令：camera_look_at、camera_screen_split、photo_storage_set、video_storage_set、camera_exposure_set、camera_exposure_mode_set、camera_focus_mode_set、camera_focus_value_set、ir_metering_mode_set、ir_metering_point_set、ir_metering_area_set、camera_point_focus_action、camera_focal_length_set、poi_mode_enter、poi_mode_exit、poi_circle_speed_set；调用 UAVDroneSim 对应 setter 并广播 OnCommandResult，执行前校验载荷权（未抢占返回 NoAuthority）。

#### Scenario: 分屏指令
- **WHEN** 收到 camera_screen_split 且 data.enable_screen_split=true
- **THEN** UAVDroneSim 分屏使能为 true，回 result=0

#### Scenario: 存储位置指令
- **WHEN** 收到 photo_storage_set 且 data.storage_location=current
- **THEN** UAVDroneSim 照片存储位置为 current，回 result=0

#### Scenario: 曝光设置指令
- **WHEN** 收到 camera_exposure_mode_set 且 data.exposure_mode=0
- **THEN** UAVDroneSim 曝光模式为 0，回 result=0

#### Scenario: 看点指令
- **WHEN** 收到 camera_look_at 且 data 含经纬度/海拔
- **THEN** UAVDroneSim 看点目标被记录，回 result=0

#### Scenario: POI 环绕指令
- **WHEN** 收到 poi_mode_enter 且 data 含经纬度/海拔
- **THEN** UAVDroneSim POI 环绕模式为 true，回 result=0

