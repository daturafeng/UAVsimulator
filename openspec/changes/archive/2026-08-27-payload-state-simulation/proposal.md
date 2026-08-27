## Why

已归档的 mqtt-bridge-integration 建立了 OSD 周期上报，但 BuildDroneOsdPayload() 中电量（capacity_percent 恒 100、电池温度/电压恒 0）、云台（gimbal_pitch/roll/yaw 恒 0）、摄像头（camera_mode="unknown"、photo_state/recording_state="idle"、zoom_factor 恒 1.0）全部是硬编码演示值。而 mqtt-bridge 主 spec 要求 OSD 字段结构"对齐 dock report_drone_osd.py"，dock 侧实际消费这些字段（充电电量、云台角度、录像状态、变焦倍率）用于页面展示与任务记录。当前行为与 spec 的差距是最大的：字段类型不一致（dock 用 int 枚举，模拟器用字符串）、数值不随飞行状态变化、battery 只有单电池而 dock 上报双电池、cameras 缺多个 dock 已消费的字段（record_time、ir_zoom_factor、zoom_focus_* 等），顶层还缺 gear / position_state / wind / storage 等字段。

## What Changes

- UAVDroneSim 增加载荷状态模拟：电量（初始 100%，飞行/待机按不同速率消耗，低于返航电量阈值广播低电量事件）、云台（俯仰基础角 + 周期微动、偏航跟随机头、横滚微动）、相机（camera_mode 0/1、变焦倍率、录像状态按飞行模式推导），以及累计飞行距离/时间遥测。
- 电池单元电压/温度、云台角度、风向枚举等推导逻辑提取为 UAVDroneSim 模块纯函数（UAVPayloadMath），与既有的 UAVGeoUtils / UAVFfmpegCommand 模式一致，可自动化测试。
- OSD 组装（UAVMqttBridge）改为从 UAVDroneSim 载荷状态读取电量/云台/相机数据，并把 battery（双电池）、payloads、cameras 与顶层字段（gear / position_state / wind / speaker / storage / night_lights_state / distance_limit_status / obstacle_avoidance / track_id 等）补全为 dock report_drone_osd.py 的结构与枚举口径（camera_mode/photo_state/recording_state 使用 int 枚举）。

## Capabilities

### New Capabilities
<!-- 无：载荷模拟能力建立在已有 drone-motion-simulation 与 mqtt-bridge 能力之上 -->

### Modified Capabilities
- drone-motion-simulation: 无人机本体模拟新增电量/云台/相机载荷状态与累计飞行遥测，状态随飞行推进。
- mqtt-bridge: OSD 报文中的电量/云台/摄像头字段来自模拟状态，字段结构对齐 dock report_drone_osd.py。

## Impact

- Source/UAVDroneSim：新增 UAVPayloadMath 纯函数（电量消耗、剩余时间、电池电压/温度、云台角度、风向枚举）；UAVDroneSimComponent 增加载荷配置、状态、查询/控制接口，Tick 推进电量与云台。
- Source/UAVMqttBridge：BuildDroneOsdPayload 改为读取载荷状态并补全 dock 对齐字段。
- Source/UAVDroneSim/Private/Tests：新增载荷数学自动化测试。
- 行为变化：OSD 电量随飞行下降（默认飞行 0.05%/秒、待机 0.005%/秒），云台角度随朝向与时间微动，camera 枚举从字符串改为 int；dock 页面可见真实变化的电量与云台状态。
