## Why

UAVMqttBridge 已具备通用 requests / requests_reply 关联框架，但 dock `RequestsMethodEnum` 中的航线资源、飞行区域和离线地图三类资源请求仍未覆盖。缺少这些能力时，模拟器无法完整响应航线准备和地图更新流程，也无法保存云端返回的资源元数据。

## What Changes

- 补齐 `flighttask_resource_get`、`flight_areas_get`、`offline_map_get` 协议常量、请求 data 构建和显式发布入口。
- 解析三类 `MqttReply<T>` 响应，校验资源文件名、URL、校验值和大小，成功时原子更新运行时资源状态，失败时保留最近一次成功状态。
- `flighttask_prepare` 成功后自动请求对应 `flight_id` 的航线资源。
- 支持 `flight_areas_update` 与 `offline_map_update` services 指令：回发 `output.status="sent"`，触发对应资源请求，并在有效响应后发布同步成功进度事件。
- 为请求结构、响应解析、更新指令触发数据和失败保护补充自动化测试。
- 本变更只完成资源元数据同步，不下载、解压或解析 KMZ、飞行区域 JSON、RocksDB 离线地图文件；实际航线文件消费由后续变更承担。

## Capabilities

### New Capabilities

### Modified Capabilities

- `mqtt-bridge`: 增加三类资源主动请求、资源更新指令触发、响应状态保存和同步进度事件行为。

## Impact

- `Source/UAVCore`：新增 services / requests / events method 常量。
- `Source/UAVMqttBridge`：扩展资源状态、请求 API、响应解析、services 分发与同步事件。
- `Source/UAVCore/Private/Tests`、`Source/UAVMqttBridge/Private/Tests`：增加协议常量和资源请求流程测试。
- 不新增第三方依赖，不持久化资源凭据或文件内容，不修改 dock 仓库。
