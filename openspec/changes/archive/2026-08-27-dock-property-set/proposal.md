## Why

模拟器无人机 OSD 已上报 height_limit / distance_limit_status / obstacle_avoidance / rth_altitude / rc_lost_action / exit_wayline_when_rc_lost / night_lights_state，机场 OSD 已上报 user_experience_improvement，但这些字段全部硬编码，且模拟器未订阅 dock 下发的物模型属性设置 topic（thing/product/{sn}/property/set）。dock-fe 设备设置页保存后，后端 DeviceServiceImpl.devicePropertySet 校验无人机 OSD 并调用 AbstractPropertyService.propertySet 下发 property/set（对齐 PropertySetFieldEnum：night_lights_state / height_limit / distance_limit_status / obstacle_avoidance / rth_altitude / rc_lost_action），模拟器无订阅无回执，导致设置保存无结果、OSD 数值不变，dock-fe 设置页无法感知属性变更。

## What Changes

- UAVCore：新增物模型 topic 常量 thing/product/{sn}/property/set 与 thing/product/{sn}/property/set_reply，对齐 dock TopicConst（THING_MODEL_PRE + PRODUCT + sn + PROPERTY_SUF + SET_SUF / _REPLY_SUF）。
- UAVMqttBridge：新增无人机/机场可设置属性状态结构（FUAVDroneProperties / FUAVDockProperties，BlueprintReadWrite），默认值对齐当前 OSD 硬编码（height_limit=500、distance_limit=3000、obstacle_avoidance 全开、rth_altitude=60、rc_lost_action=2、exit_wayline_when_rc_lost=1、night_lights_state=0、user_experience_improvement=2）。
- UAVMqttBridge：新增 property/set 订阅与 OnPropertySetMessage / DispatchPropertySetMessage / HandlePropertySet 入口，解析 data 单属性对象并校验字段（对齐后端 Receiver.valid 与 PropertySetReplyResultEnum），成功回发 property/set_reply（data:{result:0}），参数非法回 result=1。
- UAVMqttBridge：OSD 联动 —— 无人机 OSD 的 7 个属性字段与机场 OSD 的 user_experience_improvement 改为读取属性状态，不再硬编码。
- 自动化测试：新增 UAVPropertySetTests.cpp 覆盖订阅解析、各属性校验（合法/越界/缺字段）、回执结构与 OSD 联动。

## Capabilities

### New Capabilities

### Modified Capabilities
- mqtt-bridge: 新增物模型属性设置支持（property/set 订阅、属性校验与存储、property/set_reply 回执、OSD 属性联动）。

## Impact

- Source/UAVCore：UAVCloudApiTypes.h/.cpp 新增 property/set 与 property/set_reply topic 常量。
- Source/UAVMqttBridge：UAVMqttBridgeComponent 新增属性状态结构、订阅与分发/校验/回执逻辑，OSD 构建改为读取属性状态。
- Source/UAVMqttBridge/Private/Tests：新增 UAVPropertySetTests.cpp。
- 行为变化：dock-fe 设备设置页保存属性后模拟器回执 result=0 且 OSD 数值随之更新，设置页可感知属性变更。
