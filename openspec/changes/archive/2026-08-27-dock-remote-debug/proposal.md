## Why

模拟器已覆盖 services 通道的飞控/相机/DRC 指令，但 dock 的远程调试与设备控制指令面仍缺失：debug_mode_open、cover_open、drone_open、charge_open 等 20 个调试指令统一回 UnknownMethod，远程调试进度事件（RemoteDebugProgress）无法上报，机场 OSD 中 supplement_light_state / alarm_state / putter_state / battery_store_mode / air_conditioner 等字段为静态值。dock-fe 远程调试页面与 dock 后端 AbstractDebugService 已完整支持该指令面，联调时无法在虚拟座舱中开关舱门、推杆、补光灯或进入调试模式。

## What Changes

- UAVCore：新增 20 个远程调试/设备控制 method 常量（debug_mode_open / debug_mode_close / supplement_light_open / supplement_light_close / device_reboot / drone_open / drone_close / drone_format / device_format / cover_open / cover_close / putter_open / putter_close / charge_open / charge_close / battery_maintenance_switch / alarm_state_switch / battery_store_mode_switch / sdr_workmode_switch / air_conditioner_mode_switch）；进度事件 method 与 services method 同名，复用指令常量。
- UAVMqttBridge：新增机场设备模拟状态（调试模式、补光灯、报警、推杆、舱门、充电、电池存储模式、空调模式、电池保养、链路工作模式、无人机电源）与 HandleDebugCommand 指令入口；DispatchServicesMessage 精确分发远程调试指令，成功后回发 services_reply（data:{result, output:{status:"sent"}}，对齐 RemoteDebugResponse）。
- UAVMqttBridge：对带进度的方法（device_reboot / drone_open / drone_close / drone_format / device_format / cover_open / cover_close / putter_open / putter_close / charge_open / charge_close）发布 RemoteDebugProgress 进度事件（data:{result, output:{status, progress:{percent, currentStep, totalSteps, stepKey, stepResult}}}，sent → ok 两段）。
- UAVMqttBridge：机场 OSD 字段由静态值改为响应驱动状态：mode_code（调试模式为 REMOTE_DEBUGGING=2）、supplement_light_state、alarm_state、putter_state、battery_store_mode、air_conditioner.air_conditioner_state、cover_state（指令覆盖优先于归巢推导）、drone_charge_state.state（指令覆盖优先于电量推导）、drone_battery_maintenance_info.maintenance_state、wireless_link.link_workmode、sub_device.device_online_status（无人机电源）。
- 自动化测试：新增 UAVRemoteDebugTests.cpp 覆盖指令处理、回执结构、进度事件结构与 OSD 状态联动。

## Capabilities

### New Capabilities

### Modified Capabilities
- mqtt-bridge: 新增远程调试指令分发（services 精确匹配 20 个调试 method）、RemoteDebugProgress 进度事件上报与机场 OSD 设备状态联动。

## Impact

- Source/UAVCore：UAVCloudApiTypes.h/.cpp 新增 20 个 method 常量。
- Source/UAVMqttBridge：UAVMqttBridgeComponent 新增机场设备状态成员、HandleDebugCommand / IsRemoteDebugMethod、BuildRemoteDebugProgressEventData 测试入口，DispatchServicesMessage / PublishServicesReply / BuildDockOsdPayload 扩展。
- Source/UAVMqttBridge/Private/Tests：新增 UAVRemoteDebugTests.cpp。
- 行为变化：dock-fe 远程调试页面可执行调试指令，OSD 实时反映舱门/推杆/补光灯/报警/充电/空调/存储模式等状态，调试进度通过 events 上报。
