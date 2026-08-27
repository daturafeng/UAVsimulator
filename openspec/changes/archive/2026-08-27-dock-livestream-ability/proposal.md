## Why

模拟器缺少 dock 侧直播能力上报（live_capacity state）：dock 的 `SDKLivestreamService.dockLivestreamAbilityUpdate` 依赖 `thing/product/{机场SN}/state` 携带 `data.live_capacity` 建立直播能力缓存（`capacity_camera` 表）并触发 `startLivestreamByCapacity`。当前模拟器只发 OSD / 控制源 / 在线状态，dock 侧直播能力列表为空，直播指令链路依赖的目录/能力数据缺失。

dock 侧口径（`DockLiveCapacity` + `report_live_capacity.py`）：state 报文 data 为 `{ live_capacity: { available_video_number, coexist_video_number_max, device_list } }`，device_list 含网关（机场）与无人机两个设备项，各含 camera_list（camera_index / available_video_number / coexist_video_number_max / video_list），video 项含 video_index / video_type / switchable_video_types。JSON 采用 snake_case（dock-be 全局 Jackson SNAKE_CASE 策略）。

## What Changes

- UAVMqttBridge 新增直播能力 state 上报：连接成功后向 `thing/product/{DockSn}/state` 发布 `data.live_capacity` 报文，字段名与结构对齐 dock `DockLiveCapacity` 与联调脚本 `report_live_capacity.py`。
- 网关设备项：sn=DockSn，camera_list 一个 165-0-7 相机（normal-0 / normal）。
- 无人机设备项：sn=DroneSn，camera_list 含 176-0-0 普通相机（normal-0 / normal）与 CameraIndex 主载荷相机（normal-0 / zoom，可切换 normal/wide/zoom/ir）。
- UAVMqttBridge 自动化测试新增：直播能力报文结构断言（顶层 live_capacity、设备列表、相机/视频字段完整性）。

## Capabilities

### New Capabilities
<!-- 无：能力建立在已有 mqtt-bridge 之上 -->

### Modified Capabilities
- mqtt-bridge: 新增直播能力 state 上报，连接成功即发布 live_capacity 能力报文，字段集对齐 dock DockLiveCapacity。

## Impact

- Source/UAVMqttBridge：UAVMqttBridgeComponent 新增直播能力报文组装与发布（OnMqttConnect 连接成功后调用）。
- Source/UAVMqttBridge/Private/Tests：新增直播能力结构测试。
- 行为变化：dock 端直播能力缓存建立，直播列表可展示；Topic 为既有 state topic，无协议与订阅变化。
