# mqtt-bridge Specification

## ADDED Requirements

### Requirement: 直播能力上报
UAVMqttBridge MUST 在 MQTT 连接成功后向 thing/product/{机场SN}/state 发布直播能力报文，data 为 { live_capacity: { available_video_number, coexist_video_number_max, device_list } }，字段名与结构对齐 dock DockLiveCapacity 与联调脚本 report_live_capacity.py（snake_case）。device_list MUST 含两个设备项：网关设备项（sn=机场SN，camera_list 含 165-0-7 相机，video_list 为 normal-0/normal）与无人机设备项（sn=无人机SN，camera_list 含 176-0-0 普通相机 normal-0/normal 与相机索引相机 normal-0/zoom，可切换类型 normal/wide/zoom/ir）；available_video_number 为视频流总数，coexist_video_number_max 与设备/相机项一致。

#### Scenario: 连接成功后发布直播能力
- **WHEN** MQTT 连接成功
- **THEN** 向 thing/product/{机场SN}/state 发布 data.live_capacity 报文，包含 available_video_number / coexist_video_number_max / device_list

#### Scenario: 直播能力结构完整
- **WHEN** 组装直播能力报文
- **THEN** device_list 含网关与无人机两个设备项，各含 sn / available_video_number / coexist_video_number_max / camera_list；相机项含 camera_index / available_video_number / coexist_video_number_max / video_list；视频项含 video_index / video_type / switchable_video_types

#### Scenario: 无人机主载荷可切换类型
- **WHEN** 组装无人机设备项的相机索引（52-0-0）视频列表
- **THEN** video_type 为 zoom、switchable_video_types 为 [normal, wide, zoom, ir]
