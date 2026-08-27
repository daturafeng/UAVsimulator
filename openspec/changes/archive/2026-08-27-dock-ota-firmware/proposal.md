## Why

模拟器已覆盖 services 通道的飞控/相机/DRC/远程调试指令，但固件升级能力仍缺失：dock-fe 固件管理页发起升级后，模拟器对 ota_create 指令统一回 UnknownMethod，无法上报 ota_progress 进度事件，dock 侧 DeviceFirmwareServiceImpl 依赖 ota_progress 缓存升级进度并推送 WebSocket 到前端；固件版本（dock_firmware_version / rc_and_drone_firmware_version / payload 固件）也未在 state 通道上报，dock-fe 设备详情页的固件版本与升级状态为空。

## What Changes

- UAVCore：新增 ota_create services method 常量与 ota_progress 事件常量，对齐 dock FirmwareMethodEnum / EventsMethodEnum。
- UAVMqttBridge：新增 HandleOtaCreate 指令入口（解析 devices 数组、校验设备 SN/版本/固件类型，返回 result），DispatchServicesMessage 精确分发 ota_create，成功后回发 services_reply（data:{result, output:{status:"sent"}}，对齐 ServicesReplyData<OtaCreateResponse>）。
- UAVMqttBridge：新增 BuildOtaProgressEventData 测试入口并发布 ota_progress 进度事件（data:{result, output:{status, progress:{percent, current_step}, ext:{rate}}}，sent → in_progress → ok 序列，对齐 EventsDataRequest<OtaProgress>）。
- UAVMqttBridge：连接成功与升级完成后上报固件版本 state：机场 dock_firmware_version（firmware_version / compatible_status / firmware_upgrade_status）、无人机 rc_and_drone_firmware_version（firmware_version）与载荷固件（payload 索引映射），对齐 dock StateDataKeyEnum / DockFirmwareVersion / FirmwareVersion / PayloadFirmwareVersion。
- 自动化测试：新增 UAVOtaTests.cpp 覆盖指令校验、回执结构、进度事件序列与固件版本 state 结构。

## Capabilities

### New Capabilities

### Modified Capabilities
- mqtt-bridge: 新增固件升级指令分发（ota_create）、ota_progress 进度事件上报与固件版本 state 上报。

## Impact

- Source/UAVCore：UAVCloudApiTypes.h/.cpp 新增 ota_create / ota_progress 常量与固件版本常量。
- Source/UAVMqttBridge：UAVMqttBridgeComponent 新增 HandleOtaCreate / BuildOtaProgressEventData / 固件版本上报逻辑，DispatchServicesMessage 与连接建立流程扩展。
- Source/UAVMqttBridge/Private/Tests：新增 UAVOtaTests.cpp。
- 行为变化：dock-fe 固件管理页可对模拟机场发起升级，升级进度经 ota_progress 事件推送到前端，设备详情页展示固件版本与升级状态。
