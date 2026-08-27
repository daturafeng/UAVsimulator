## Why

已归档的 payload-control-auto-rth 让 UAVCameraStream 能处理 payload_authority_grab、camera_mode_switch、camera_photo_take/stop、camera_recording_start/stop、camera_aim、gimbal_reset 等基础载荷指令，但 dock 的 simulate_control_services.py 明确支持的相机设置类指令（camera_look_at、camera_screen_split、photo_storage_set、video_storage_set、camera_exposure_set、camera_exposure_mode_set、camera_focus_mode_set、camera_focus_value_set、ir_metering_mode_set、ir_metering_point_set、ir_metering_area_set、camera_point_focus_action、camera_focal_length_set、poi_mode_enter、poi_mode_exit、poi_circle_speed_set）仍统一回 UnknownMethod，联调时 dock 下发的这些指令全部失败。同时 OSD 中 screen_split_enable、photo_storage_settings、video_storage_settings、zoom_focus_mode/value 目前是硬编码常量，无法反映载荷状态。

## What Changes

- UAVDroneSim 扩展相机设置状态：曝光模式/快门/ISO/曝光补偿、对焦模式/对焦值/点对焦动作、红外测光模式/测光点/测光区域、照片/录像存储位置、分屏使能、焦距、看点目标（经纬度/海拔）、POI 环绕模式与环绕速度；提供指令 setter 与查询 getter。
- UAVCameraStream 处理相机设置指令：camera_look_at、camera_screen_split、photo_storage_set、video_storage_set、camera_exposure_set、camera_exposure_mode_set、camera_focus_mode_set、camera_focus_value_set、ir_metering_mode_set、ir_metering_point_set、ir_metering_area_set、camera_point_focus_action、camera_focal_length_set、poi_mode_enter、poi_mode_exit、poi_circle_speed_set；除 payload_authority_grab 外的载荷指令仍校验载荷权，结果经 OnCommandResult 广播回 services_reply。
- UAVMqttBridge 载荷指令分发扩展：新增 photo_storage_set / video_storage_set / ir_metering_* / poi_* 前缀分发到 UAVCameraStream；OSD cameras 字段接入实时状态（screen_split_enable、photo/video_storage_settings、zoom_focus_mode/value）。
- UAVCore 新增相机设置指令 method 常量。

## Capabilities

### New Capabilities
<!-- 无：能力建立在已有 camera-streaming / drone-motion-simulation / mqtt-bridge 之上 -->

### Modified Capabilities
- camera-streaming: 相机设置指令（曝光/对焦/测光/存储/分屏/看点/POI）从未知回非 0 变为可执行并驱动载荷状态。
- drone-motion-simulation: 相机设置状态可被指令控制（曝光/对焦/测光/存储/分屏/看点/POI）。
- mqtt-bridge: 载荷指令分发扩展与 OSD 相机设置字段实时化。

## Impact

- Source/UAVDroneSim：UAVDroneSimComponent 增加相机设置状态接口（曝光/对焦/测光/存储/分屏/焦距/看点/POI）。
- Source/UAVCameraStream：UAVCameraStreamComponent 增加相机设置指令处理函数（复用载荷权校验与 OnCommandResult）。
- Source/UAVMqttBridge：DispatchServicesMessage 分发前缀扩展；OSD cameras 组装接入实时设置值。
- Source/UAVCore：UAVCloudApiTypes 增加相机设置指令 method 常量。
- Source/UAVDroneSim/Private/Tests：新增相机设置状态自动化测试。
- 行为变化：dock 下发相机设置指令后模拟器回 0 并驱动载荷状态，OSD 相机字段反映最新设置；POI 指令记录环绕模式与速度（不改变飞行物理）。

