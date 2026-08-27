# camera-streaming Specification

## Purpose

UAVCameraStream 在基础载荷指令之外，处理相机设置指令（曝光/对焦/测光/存储/分屏/看点/POI），驱动 UAVDroneSim 相机设置状态。

## ADDED Requirements

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

