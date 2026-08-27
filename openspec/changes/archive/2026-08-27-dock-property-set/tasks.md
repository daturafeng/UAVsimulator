## 1. 协议常量

- [x] 1.1 UAVCore：UAVCloudApiTypes.h/.cpp 新增 property/set 与 property/set_reply topic 常量（thing/product/{sn}/property/set、thing/product/{sn}/property/set_reply）

## 2. 属性状态与订阅分发

- [x] 2.1 UAVMqttBridge：新增 FUAVDroneProperties / FUAVDockProperties 状态结构（默认值对齐当前 OSD 硬编码）
- [x] 2.2 UAVMqttBridge：OnMqttConnect 新增 property/set 订阅，新增 OnPropertySetMessage / DispatchPropertySetMessage（解析 tid/bid/data 单属性并回发 property/set_reply）
- [x] 2.3 UAVMqttBridge：新增 HandlePropertySet 入口，按属性名校验并写入状态（7 无人机属性 + 1 机场属性），返回 result

## 3. OSD 联动

- [x] 3.1 UAVMqttBridge：BuildDroneOsdPayload 的 night_lights_state / height_limit / distance_limit_status / obstacle_avoidance / rc_lost_action / rth_altitude / exit_wayline_when_rc_lost 改为读取属性状态
- [x] 3.2 UAVMqttBridge：BuildDockOsdPayload 的 user_experience_improvement 改为读取属性状态

## 4. 自动化测试

- [x] 4.1 UAVMqttBridge/Tests：新增 UAVPropertySetTests.cpp 断言 7 无人机属性 + 1 机场属性合法设置更新状态并回执 result=0
- [x] 4.2 UAVMqttBridge/Tests：断言越界/缺字段/未知属性回执 result=1 且状态不变
- [x] 4.3 UAVMqttBridge/Tests：断言属性设置后 OSD 联动与默认属性值输出

## 5. 构建与验证

- [x] 5.1 UBT 构建 UAVsimulatorEditor（Win64 Development）
- [x] 5.2 Automation RunTests UAV 全部通过（含新增用例与既有基线）
- [x] 5.3 openspec validate --specs 通过，按流程归档变更并 git commit + push
