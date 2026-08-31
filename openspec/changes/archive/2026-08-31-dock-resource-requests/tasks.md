## 1. 协议基础

- [x] 1.1 UAVCore：新增 flight_areas_update / offline_map_update services、三类 resource requests 与两个 sync_progress events 常量
- [x] 1.2 UAVCore/Tests：断言新增 method 常量与 requests 报文结构

## 2. 资源请求与状态

- [x] 2.1 UAVMqttBridge：新增航线资源、通用资源文件、飞行区域、离线地图 Blueprint 只读状态与 getter
- [x] 2.2 UAVMqttBridge：新增三类 request data 构建器和显式发布 API，空 flight_id 不创建 pending
- [x] 2.3 UAVMqttBridge：flighttask_prepare 成功后自动请求对应航线资源
- [x] 2.4 UAVMqttBridge：pending 记录请求上下文、创建时间和同步进度标志

## 3. 响应解析与更新流程

- [x] 3.1 UAVMqttBridge：解析 flighttask_resource_get，原子保存 flight_id / url / fingerprint，失败保留旧状态
- [x] 3.2 UAVMqttBridge：校验并解析 flight_areas_get 文件列表，支持合法空数组，失败保留旧状态
- [x] 3.3 UAVMqttBridge：校验并解析 offline_map_get 启用状态与文件列表，失败保留旧状态
- [x] 3.4 UAVMqttBridge：处理 flight_areas_update / offline_map_update，回发 sent 并触发带同步上下文的资源请求
- [x] 3.5 UAVMqttBridge：组装 synchronized / fail 同步事件，并仅为更新流程请求发布
- [x] 3.6 UAVMqttBridge：按 RequestTimeoutSeconds 清理超时 pending，广播失败并按上下文发布 fail 事件

## 4. 自动化测试

- [x] 4.1 UAVMqttBridge/Tests：覆盖三类 request data、flight_id 校验与状态 getter
- [x] 4.2 UAVMqttBridge/Tests：覆盖三类成功响应、空列表、文件名/字段/size 非法及旧状态保护
- [x] 4.3 UAVMqttBridge/Tests：覆盖 update 指令同步上下文、sync_progress 结构和超时清理
- [x] 4.4 运行完整 Automation RunTests UAV，确认新增与既有测试全部通过

## 5. 构建与交付

- [x] 5.1 UBT 构建 UAVsimulatorEditor（Win64 Development）
- [x] 5.2 openspec validate --specs 通过，同步主规格并归档变更
- [x] 5.3 排除 Config/DefaultEngine.ini 后 git commit + push，核对本地与 origin/main 哈希
