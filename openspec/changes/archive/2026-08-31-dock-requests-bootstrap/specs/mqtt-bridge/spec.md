## ADDED Requirements

### Requirement: 设备主动 requests 通道
UAVMqttBridge MUST 在 MQTT 连接成功后订阅 thing/product/{机场SN}/requests_reply，并能向 thing/product/{机场SN}/requests 发布设备主动请求。请求报文 MUST 含非空 tid / bid、毫秒时间戳、gateway=机场SN、method 与 data；每个请求 MUST 以 tid / bid / method 记录为待处理请求。requests_reply 仅在 tid / bid / method 均与待处理请求一致时更新业务状态并完成该请求；未知 bid 或字段不匹配的响应 MUST 被忽略且不得更新业务状态。

#### Scenario: 主动请求报文结构
- **WHEN** 模拟器发布 method=config、data={config_type:"json", config_scope:"product"} 的请求
- **THEN** thing/product/{机场SN}/requests 报文含非空 tid / bid、timestamp、gateway=机场SN、method=config 与原始 data

#### Scenario: 响应关联成功
- **WHEN** requests_reply 的 tid / bid / method 与待处理请求完全一致
- **THEN** 模拟器按 method 解析响应、移除待处理请求并广播请求完成事件

#### Scenario: 未知或错配响应
- **WHEN** requests_reply 的 bid 不存在，或 tid / method 与待处理请求不一致
- **THEN** 模拟器忽略该响应，不移除原待处理请求且不更新任何业务状态

### Requirement: 上线配置与绑定状态请求
UAVMqttBridge MUST 在 MQTT 连接成功且 requests_reply 订阅完成后发布 config 与 airport_bind_status 两个启动请求。config 的 data MUST 为 { config_type: "json", config_scope: "product" }；airport_bind_status 的 data.devices MUST 含机场 SN 与无人机 SN 两项。config 成功响应 data MUST 解析 ntp_server_host / app_id / app_key / app_license；airport_bind_status 成功响应 MUST 解析 data={result:0, output:{bind_status:[...]}}，每项按 sn 记录 is_device_bind_organization / organization_id / organization_name / device_callsign。

#### Scenario: 连接成功启动请求
- **WHEN** MQTT 连接成功且 requests_reply 已订阅
- **THEN** 模拟器依次发布 config 与 airport_bind_status 请求，绑定状态请求的 devices 含机场和无人机 SN

#### Scenario: 配置响应落地
- **WHEN** 收到匹配的 config 响应，data 含 ntp_server_host / app_id / app_key / app_license
- **THEN** 模拟器记录最新产品配置并广播 config 请求成功

#### Scenario: 绑定状态响应落地
- **WHEN** 收到匹配的 airport_bind_status 响应，data.result=0 且 output.bind_status 含机场与无人机状态
- **THEN** 模拟器按 SN 记录两台设备的组织绑定状态、组织标识、组织名称与呼号

### Requirement: 未绑定设备组织握手
UAVMqttBridge MUST 在 airport_bind_status 成功响应表明机场或无人机未绑定时，仅当 device_binding_code 非空才发布 airport_organization_get，请求 data 含 device_binding_code 与 organization_id。airport_organization_get 成功响应 data={result:0, output:{organization_name}} 后，MUST 发布 airport_organization_bind，请求 data.bind_devices 含机场与无人机两项；每项含 device_binding_code / organization_id / device_callsign / sn / device_model_key，默认机场 model key 为 "3-3-0"、无人机 model key 为 "0-100-1"。airport_organization_bind 成功响应 MUST 解析 output.err_infos，只有两台设备 err_code 均为 0 时组织绑定状态才标记成功。

#### Scenario: 未绑定时查询组织
- **WHEN** airport_bind_status 返回任一设备未绑定且 device_binding_code 已配置
- **THEN** 模拟器发布 airport_organization_get 请求，data 使用配置的绑定码与组织标识

#### Scenario: 未配置绑定码
- **WHEN** airport_bind_status 返回未绑定但 device_binding_code 为空
- **THEN** 模拟器不发布 airport_organization_get 或 airport_organization_bind

#### Scenario: 查询组织后发起绑定
- **WHEN** airport_organization_get 匹配响应 data.result=0 且 output.organization_name 非空
- **THEN** 模拟器记录组织名称，并发布包含机场 "3-3-0" 与无人机 "0-100-1" 的 airport_organization_bind 请求

#### Scenario: 组织绑定成功
- **WHEN** airport_organization_bind 匹配响应 data.result=0，output.err_infos 含机场与无人机且每项 err_code=0
- **THEN** 模拟器将机场与无人机组织绑定状态标记为成功并广播请求完成

### Requirement: 对象存储配置请求
UAVMqttBridge MUST 提供显式 storage_config_get 请求入口，请求 data 为 { module: 0 }（MEDIA）。成功响应 MUST 解析 data={result:0, output:{bucket, credentials, endpoint, object_key_prefix, provider, region}} 并记录最新对象存储配置；result 非 0 或 output 缺少必填字段时 MUST 广播请求失败且不得覆盖最近一次成功配置。

#### Scenario: 请求媒体对象存储配置
- **WHEN** 调用对象存储配置请求入口
- **THEN** 模拟器向 requests topic 发布 method=storage_config_get、data.module=0 的请求

#### Scenario: 对象存储配置成功
- **WHEN** 收到匹配的 storage_config_get 响应，data.result=0 且 output 必填字段齐全
- **THEN** 模拟器记录 bucket / credentials / endpoint / object_key_prefix / provider / region 并广播请求成功

#### Scenario: 对象存储配置失败
- **WHEN** storage_config_get 响应 result 非 0 或 output 字段缺失
- **THEN** 模拟器广播请求失败且保留最近一次成功的对象存储配置
