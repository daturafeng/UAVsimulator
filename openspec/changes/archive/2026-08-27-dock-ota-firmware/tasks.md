## 1. 协议常量

- [x] 1.1 UAVCore：UAVCloudApiTypes.h/.cpp 新增 ota_create services method 常量、ota_progress 事件常量；固件版本 state 无独立 method 常量（firmware_version 为 data 键，由桥接组件构建报文）

## 2. 指令处理与进度事件

- [x] 2.1 UAVMqttBridge：新增升级状态私有成员（OtaTargetVersion、bOtaUpgrading）与 HandleOtaCreate（解析 devices、校验设备项、更新目标版本、返回 result）
- [x] 2.2 UAVMqttBridge：DispatchServicesMessage 精确分发 ota_create，成功回执 data:{result, output:{status:"sent"}}，失败仅 result
- [x] 2.3 UAVMqttBridge：新增 BuildOtaProgressEventData 测试入口（result/output.status/progress.percent/current_step/ext.rate），成功后发布 sent → in_progress → ok 三段事件

## 3. 固件版本 state 上报

- [x] 3.1 UAVMqttBridge：新增 BuildDockFirmwareVersionData / BuildDroneFirmwareVersionData / BuildPayloadFirmwareVersionData 测试入口
- [x] 3.2 UAVMqttBridge：连接成功流程扩展为发布机场/无人机/载荷固件版本 state；升级完成（ok 事件）后更新版本并重发

## 4. 自动化测试

- [x] 4.1 UAVMqttBridge/Tests：新增 UAVOtaTests.cpp 断言 ota_create 指令处理与参数校验（devices 缺失/字段非法/升级类型越界）
- [x] 4.2 UAVMqttBridge/Tests：断言 services_reply output.status、进度事件 sent/in_progress/ok 序列与结构
- [x] 4.3 UAVMqttBridge/Tests：断言固件版本 state 结构与升级完成后的版本更新

## 5. 构建与验证

- [x] 5.1 UBT 构建 UAVsimulatorEditor（Win64 Development）
- [x] 5.2 Automation RunTests UAV 全部通过（含新增用例与既有基线）
- [x] 5.3 openspec validate --specs 通过，按流程归档变更并 git commit + push

