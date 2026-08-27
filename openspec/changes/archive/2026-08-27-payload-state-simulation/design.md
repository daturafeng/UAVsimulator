## Context

UUAVDroneSimComponent（UAVDroneSim）目前只维护位置/朝向/速度与航点队列，无电量与载荷概念；UUAVMqttBridgeComponent::BuildDroneOsdPayload() 把电量、云台、摄像头字段写成固定演示值。dock 侧 report_drone_osd.py（D:\WebCode\dock\script\report_drone_osd.py）定义完整 OSD 结构：battery 双电池（index/temperature/voltage）、capacity_percent int、landing_power 20、return_home_power 25、remain_flight_time；payloads 数组（gimbal_pitch/roll/yaw、zoom_factor）；cameras 数组（camera_mode=1、photo_state=0、recording_state 0/1、zoom_factor、ir_zoom_factor、zoom_focus_*、record_time、liveview_world_region、storage settings 等）；顶层还有 gear、position_state、wind_direction/wind_speed、total_flight_distance/time、speaker、storage、night_lights_state、height_limit、distance_limit_status、obstacle_avoidance、rc_lost_action、rth_altitude、track_id 等。

## Goals / Non-Goals

**Goals:**

- 电量随飞行状态消耗并可查询/上报，低电量（低于返航阈值）广播事件。
- 云台角度模拟：俯仰基础角 + 周期微动，偏航默认跟随机头，横滚小幅度微动。
- 相机状态模拟：camera_mode（0 拍照/1 录像）、变焦倍率（可配置范围）、录像状态按飞行模式推导（对齐 dock：模式编码 ∈ {1,4,9} 即起飞/航线/返航时录像中）。
- OSD 组装对齐 dock report_drone_osd.py：battery 双电池 + 电压/温度公式、payloads/cameras 从模拟状态读取、枚举用 int、补齐顶层结构字段。
- 推导逻辑纯函数化，自动化测试覆盖电量/电池/云台/风向。

**Non-Goals:**

- 不做低电量自动返航（空电返航联动）：仅广播低电量事件，联动由后续飞控变更承接。
- 不做拍照/录像指令（camera_photo / camera_record）的 services 业务实现，仅提供状态模拟与 OSD 上报。
- 不新增 UAVMqttBridge 配置（landing/return_home 电量阈值统一由 UAVDroneSim 提供）。

## Decisions

- **载荷状态归属 UAVDroneSim 组件**：电量/云台/相机是无人机本体模拟的一部分，直接扩展 UUAVDroneSimComponent（新增配置、状态、查询接口），不新建模块，避免破坏既有模块职责划分。
- **推导逻辑提取为 UAVPayloadMath 纯函数**：与 UAVGeoUtils / UAVFfmpegCommand 既有模式一致。新增 Public/UAVPayloadMath.h（含 FUAVGimbalConfig / FUAVGimbalState 结构与命名空间 UAVPayloadMath 静态函数）与 Private/UAVPayloadMath.cpp，便于自动化测试与复用。
- **电量模型**：BatteryCapacityStartPercent=100；飞行（TakingOff/Wayline/Flying/ReturnHome/Landing）按 BatteryDrainPercentPerSecond=0.05 消耗，待机（Idle）按 BatteryIdleDrainPercentPerSecond=0.005 消耗；remain_flight_time = 当前电量/飞行消耗速率；landing_power=20、return_home_power=25 对齐 dock。
- **电池单元公式对齐 dock**：温度 = 27.5 + (100-电量)*0.06 + 水平速度*0.18（± 微动），电压 = max(22000, 25800 + 电量*18 - 水平速度*35) 毫伏；双电池第二个单元温度 +0.5、电压 -80。
- **云台模型**：pitch = 基础角(-8°) + sin(时间*0.24)*6°；roll = cos(时间*0.22)*1.2°；yaw = 跟随机头时 (朝向 + sin(时间*0.28)*4°) 归一化到 0-360。时间源为组件累计模拟秒，保证确定性。
- **相机模型**：CameraMode 默认 1（录像，int 枚举对齐 dock）；ZoomFactor 默认 3.0、钳制到 [1.0, 7.0]；IsRecording 由飞行状态推导（TakingOff/Wayline/ReturnHome 为录像中，对齐 dock 模式编码集合 {1,4,9}）；录制累计秒数供 record_time 使用。
- **OSD 静态字段常量对齐 dock**：position_state（gps 18/fixed 2/quality 4/rtk 14）、wind_direction 由朝向按 8 方位枚举推导、wind_speed 常量 3.0、speaker 0、storage total 131072MB（used 按录制时长推导）、height_limit 500、distance_limit_status、obstacle_avoidance 1/1/1、rc_lost_action 2、rth_altitude 60、country CN、track_id SIM-{sn} 等。
- **OSD 双电池与顶层字段全部在本变更补齐**：直接对齐 dock 结构，bridge 只从 DroneSim 读取模拟状态与电池阈值配置，不重复配置。

## Risks / Trade-offs

- [电量速率是演示口径] → 默认值可配置（EditAnywhere），联调时可按 dock 期望调整。
- [云台微动用时间正弦近似] → 与 dock 假设备的"轮次正弦"模式一致，均为演示级变化；真实云台指令联动留待后续变更。
- [camera_mode 由字符串改为 int] → 与 dock 解析一致，但可能影响现有依赖字符串的消费方；仓库内无消费方，dock 侧按 int 解析，风险可控。

## Open Questions

无。
