## Context

上一变更 payload-control-auto-rth 完成后，UAVCameraStream 已能处理 payload_authority_grab / camera_mode_switch / camera_photo_take|stop / camera_recording_start|stop / camera_aim / gimbal_reset，且已实现载荷权校验、UAVDroneSim 载荷状态（拍照/录像覆盖/云台目标/载荷权）、OSD 载荷字段。当前缺口：dock 的 simulate_control_services.py 支持自动成功的方法集合中，仍有 16 个相机设置类 method（camera_look_at、camera_screen_split、photo_storage_set、video_storage_set、camera_exposure_set、camera_exposure_mode_set、camera_focus_mode_set、camera_focus_value_set、ir_metering_mode_set、ir_metering_point_set、ir_metering_area_set、camera_point_focus_action、camera_focal_length_set、poi_mode_enter、poi_mode_exit、poi_circle_speed_set）未被 UAVsimulator 处理，统一回 UnknownMethod（result=1），联调时这些指令全部失败。OSD cameras 中 screen_split_enable / photo_storage_settings / video_storage_settings / zoom_focus_mode / zoom_focus_value 是硬编码常量，无法反映载荷设置状态。

dock 侧口径（simulate_control_services.py）：
- 上述 16 个 method 均位于"支持自动成功的方法"集合内，回 result=0。
- data 字段名遵循上云 API 相机载荷协议：camera_look_at（longitude/latitude/height）、camera_screen_split（enable_screen_split）、photo_storage_set / video_storage_set（storage_location）、camera_exposure_mode_set（exposure_mode）、camera_exposure_set（shutter_speed/iso/exposure_compensation）、camera_focus_mode_set（focus_mode）、camera_focus_value_set（focus_value）、ir_metering_mode_set（ir_metering_mode）、ir_metering_point_set（ir_metering_point_x/y）、ir_metering_area_set（ir_metering_area_x/y/z/w/h）、camera_point_focus_action（point_focus_action）、camera_focal_length_set（focal_length）、poi_mode_enter（longitude/latitude/height）、poi_circle_speed_set（max_speed/gimbal_yaw_rate）。
- OSD 消费的相机设置字段：screen_split_enable、photo_storage_settings、video_storage_settings、zoom_focus_mode/value/max/min、ir_zoom_factor。

## Goals / Non-Goals

**Goals:**

- UAVDroneSim 扩展相机设置状态（曝光/对焦/测光/存储/分屏/焦距/看点/POI）并暴露 setter/getter，供指令驱动与 OSD 输出。
- UAVCameraStream 处理全部 16 个相机设置 method，沿用载荷权校验与 OnCommandResult 回包。
- UAVMqttBridge 分发前缀扩展（photo_storage_* / video_storage_* / ir_metering_* / poi_*），OSD cameras 接入实时设置值。
- 相机设置状态可测（纯逻辑自动化测试）。

**Non-Goals:**

- 不做相机设置的实际渲染/编码效果（曝光/对焦/测光仅状态记录，画面渲染属后续能力）。
- 不做 POI 环绕飞行物理（poi_mode_enter 仅记录环绕目标与速度，不改变飞行轨迹；环绕飞行属后续能力）。
- 不做 camera_look_at 的云台自动转向（仅记录看点目标坐标；云台朝向仍由 camera_aim 控制）。

## Decisions

- **相机设置状态归属 UAVDroneSim**：与拍照/录像/云台目标一致，载荷状态统一由 UAVDroneSim 维护，UAVCameraStream 只做指令解析与权限校验，避免状态分散。
- **指令前缀分发**：UAVMqttBridge 的 bIsPayloadCommand 扩展为 camera_* / payload_* / gimbal_* / photo_storage_* / video_storage_* / ir_metering_* / poi_*，全部走 UAVCameraStream.HandleCommand。
- **载荷权校验范围**：除 payload_authority_grab 外的相机设置指令同样校验载荷权，未抢权返回 NoAuthority（result=2），与既有载荷指令语义一致。
- **存储位置模型**：photo_storage_settings / video_storage_settings 为字符串数组（OSD 对齐 dock ["current"]），指令 storage_location 为字符串，setter 以数组存储、查询按首元素输出。
- **POI 模型**：poi_mode_enter 记录环绕目标（经纬度/海拔）并置 bPoiModeActive=true；poi_mode_exit 置 false；poi_circle_speed_set 记录最大环绕速度与云台偏航角速度；不驱动飞行物理（Non-Goal）。
- **看点模型**：camera_look_at 记录目标经纬度/海拔，暴露 GetLookAtTarget；不自动转向云台（Non-Goal）。
- **OSD 实时化**：screen_split_enable 输出分屏使能；photo/video_storage_settings 输出存储位置数组；zoom_focus_mode/value 输出对焦模式/对焦值（zoom_focus_state 保持 0）。
- **新增测试**：UAV.Payload.CameraSettings 覆盖曝光/对焦/测光/存储/分屏/焦距/看点/POI 状态 setter/getter 与钳制。

## Risks / Trade-offs

- [相机设置无真实硬件效果] → 状态与 OSD 表现一致即可，dock 为演示级消费，精度满足联调。
- [POI 仅记录状态] → dock 下发 poi_mode_enter 后回 0 且 OSD 不展示 POI 字段，无副作用；环绕飞行列为后续能力。
- [分发前缀扩展匹配面] → 前缀均为上云 API 固定 method 命名，无歧义；未知 method 仍回 UnknownMethod。

