## 1. 协议常量

- [x] 1.1 UAVCore：UAVCloudApiTypes.h/.cpp 新增 20 个调试/设备控制 method 常量（debug_mode_open 等）

## 2. 机场设备状态与指令处理

- [x] 2.1 UAVMqttBridge：新增机场设备状态私有成员（调试模式/补光灯/报警/推杆/舱门覆盖/充电覆盖/电池存储模式/空调模式/电池保养/链路工作模式/无人机电源）
- [x] 2.2 UAVMqttBridge：新增 HandleDebugCommand（解析 data、按 dock 枚举校验带参指令、更新状态、返回 result）与 IsRemoteDebugMethod 精确方法集合
- [x] 2.3 UAVMqttBridge：新增 BuildRemoteDebugProgressEventData 测试入口（result/output.status/progress.percent/currentStep/totalSteps/stepKey/stepResult）
- [x] 2.4 UAVMqttBridge：PublishServicesReply 增加可选 output 参数，调试指令成功回执 data:{result, output:{status:"sent"}}
- [x] 2.5 UAVMqttBridge：DispatchServicesMessage 分发远程调试指令并发布 sent → ok 进度事件（12 个带进度方法）

## 3. OSD 状态联动

- [x] 3.1 UAVMqttBridge：BuildDockOsdPayload 字段改为指令驱动（mode_code=2 调试模式、supplement_light_state、alarm_state、putter_state、battery_store_mode、air_conditioner_state、maintenance_state、link_workmode、device_online_status）
- [x] 3.2 UAVMqttBridge：cover_state / drone_charge_state.state 支持指令覆盖优先、未覆盖回退推导

## 4. 自动化测试

- [x] 4.1 UAVMqttBridge/Tests：新增 UAVRemoteDebugTests.cpp 断言指令处理与参数校验（开关/模式类非法值）
- [x] 4.2 UAVMqttBridge/Tests：断言 services_reply output.status、进度事件 sent/ok 结构与 stepKey 映射
- [x] 4.3 UAVMqttBridge/Tests：断言 OSD 字段随调试指令联动（含覆盖优先与回退推导）

## 5. 构建与验证

- [x] 5.1 UBT 构建 UAVsimulatorEditor（Win64 Development）
- [x] 5.2 Automation RunTests UAV 全部通过（含新增用例与既有基线）
- [x] 5.3 openspec validate --specs 通过，按流程归档变更并 git commit + push
