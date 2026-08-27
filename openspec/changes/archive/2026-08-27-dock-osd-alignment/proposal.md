## Why

上一变更 camera-settings-commands 完成后，无人机 OSD（thing/product/{DroneSn}/osd）已对齐 dock report_drone_osd.py 的完整字段，但机场 OSD（thing/product/{DockSn}/osd）仍只有 sn 与 drone_status 两个字段。dock 后端 OsdRouter 使用 Jackson 将机场 OSD 反序列化为 OsdDock（network_state / drone_in_dock / drone_charge_state / mode_code / cover_state / air_conditioner / sub_device / flighttask_step_code / wireless_link 等 40+ 字段），字段缺失导致 dock 设备管理页与任务编排拿不到机场环境、机库、子设备、充电与图传链路信息，联调时机场侧数据全空。

dock 侧口径（report_dock_osd.py + OsdDock.java）：机场 OSD 字段名与类型严格对齐 OsdDock，其中多数字段可由现有 UAVDroneSim 状态推导（机场位置 AirportOrigin、电量、飞行状态、累计飞行时长），少数环境/链路字段按模拟值输出。

## What Changes

- UAVMqttBridge 扩展机场 OSD 组装：PublishDockOsd 输出对齐 dock OsdDock 的完整 data（network_state、drone_in_dock、drone_charge_state、mode_code、cover_state、air_conditioner、sub_device、flighttask_step_code、wireless_link 等），数值由 UAVDroneSim 状态推导或按模拟基线输出。
- UAVMqttBridge 提供机场状态推导：drone_in_dock（无人机在机场原点附近且低高度）、drone_charge_state（归巢待命且电量未满为充电中）、flighttask_step_code（按飞行状态映射任务步骤编码）、acc_time（累计飞行时长取整）。

## Capabilities

### New Capabilities
<!-- 无：能力建立在已有 mqtt-bridge 之上 -->

### Modified Capabilities
- mqtt-bridge: 机场 OSD 从精简字段（sn/drone_status）扩展为对齐 dock OsdDock 的完整报文。

## Impact

- Source/UAVMqttBridge：UAVMqttBridgeComponent 增加机场 OSD 组装与状态推导（BuildDockOsdPayload / 辅助函数），PublishDockOsd 调用新组装逻辑。
- Source/UAVMqttBridge/Private/Tests：新增机场 OSD 组装自动化测试（结构完整、数值推导正确）。
- 行为变化：dock 订阅机场 OSD 后可解析到完整环境/机库/子设备/充电/图传字段，设备管理页与任务编排数据不再为空；无协议与 Topic 变化。
