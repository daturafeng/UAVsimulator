## 1. 插件启用与依赖

- [ ] 1.1 在 UAVsimulator.uproject 的 Plugins 中启用 MQTT（MQTTCore）
- [ ] 1.2 UAVMqttBridge.Build.cs 增加 MQTTCore 模块依赖（PublicDependency）

## 2. 报文工具（UAVCore）

- [ ] 2.1 UAVCloudApiTypes 新增 services_reply / events / state 报文构造工具（复用 MakeMessageHeader）
- [ ] 2.2 新增 OSD 报文组装辅助（无人机核心字段结构体 → FJsonObject）

## 3. MQTT 连接管理（UAVMqttBridge）

- [ ] 3.1 配置字段（broker/端口/用户名/密码/机场SN/无人机SN/相机索引）与默认值对齐 dock
- [ ] 3.2 通过 UMQTTSubsystem 创建客户端并实现 Connect/Disconnect 与连接状态事件
- [ ] 3.3 连接成功后订阅 services topic 并发布在线状态（sys status / state）

## 4. 指令分发与回复

- [ ] 4.1 解析 services 报文（tid/bid/method/data）并提取设备 SN
- [ ] 4.2 按 method 分发到 UAVFlightControl / UAVCameraStream 的 HandleCommand
- [ ] 4.3 组装并发布 services_reply（result 0/非0，未知指令回非0）

## 5. 事件转发

- [ ] 5.1 绑定飞控委托（OnCommandResult / OnTakeoffProgress / OnFlighttaskProgress）并发布 events
- [ ] 5.2 绑定相机直播状态委托（OnLiveStatusChanged）并发布 live_status 事件

## 6. OSD 遥测

- [ ] 6.1 周期（1 秒）从 UAVDroneSim 读取遥测组装无人机 OSD 并发布到 osd topic
- [ ] 6.2 发布机场 OSD（精简字段）与设备 state 报文
- [ ] 6.3 未注入模拟组件时安全跳过

## 7. 验证与归档

- [ ] 7.1 编译通过（UE 5.7 UBT 构建 UAVsimulatorEditor）
- [ ] 7.2 通过 openspec validate 校验变更
- [ ] 7.3 同步主 spec 并归档变更
