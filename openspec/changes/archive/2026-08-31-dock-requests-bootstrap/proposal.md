## Why

模拟器当前只接收 dock 下发的 services / drc / property/set 报文，尚不能像真实机场一样通过 thing/product/{sn}/requests 主动向云端获取启动配置、组织绑定状态与对象存储配置，也不订阅 requests_reply。结果是设备上线后的主动握手链路缺失，dock 的 RequestConfigContext、SDKOrganizationService 与 StorageServiceImpl 无法在联调中被模拟器触发。

## What Changes

- UAVCore：新增 requests / requests_reply topic 模板，以及 config / airport_bind_status / airport_organization_get / airport_organization_bind / storage_config_get 五个 request method 常量。
- UAVMqttBridge：新增 requests_reply 订阅、设备主动 request 报文构建与发布能力；报文统一携带 tid / bid / timestamp / gateway / method / data，并按 bid 记录待处理请求。
- UAVMqttBridge：MQTT 连接成功后发布 config 与 airport_bind_status；若响应表明设备未绑定且已配置绑定码，则继续发布 airport_organization_get，成功后发布 airport_organization_bind 完成机场与无人机绑定；storage_config_get 提供显式发布入口，供媒体上传前获取对象存储配置。
- UAVMqttBridge：解析 requests_reply 的 method / bid / data，校验与待处理请求匹配后记录最近配置、绑定状态、组织名称、绑定结果和对象存储配置，并广播请求完成事件；未知 bid 或 method 不匹配时忽略业务状态更新。
- 自动化测试：覆盖五类请求 data、统一请求报文、响应关联与状态解析、组织握手分支、未知响应保护。

## Capabilities

### New Capabilities

### Modified Capabilities
- mqtt-bridge: 新增设备主动 requests 通道、requests_reply 关联处理，以及上线配置/组织握手与对象存储配置请求。

## Impact

- Source/UAVCore/Public|Private/UAVCloudApiTypes.*：新增 topic / method 常量与 request 报文构建工具。
- Source/UAVMqttBridge/Public|Private/UAVMqttBridgeComponent.*：新增订阅、发布、待处理请求和响应状态。
- Source/UAVMqttBridge/Private/Tests：新增 requests 通道自动化测试。
- 外部协议：对齐 D:\WebCode\dock\backend\dock-be\cloud-sdk 的 RequestsMethodEnum / TopicRequestsRequest / TopicRequestsResponse，以及 sample 的 RequestConfigContext / SDKOrganizationService / StorageServiceImpl。
