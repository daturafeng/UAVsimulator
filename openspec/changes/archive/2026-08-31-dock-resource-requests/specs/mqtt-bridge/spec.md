## ADDED Requirements

### Requirement: 云端资源主动请求
UAVMqttBridge MUST 支持 `flighttask_resource_get`、`flight_areas_get` 与 `offline_map_get` 三种设备主动请求（对齐 dock `RequestsMethodEnum`）。`flighttask_resource_get` 的 data MUST 为 `{flight_id}` 且 flight_id 非空；另外两种请求的 data MUST 为空对象。三种请求 MUST 复用现有 tid / bid / method 精确关联机制并提供显式发布入口。`flighttask_prepare` 指令处理成功后 MUST 自动为该指令的 flight_id 发布一次 `flighttask_resource_get`。

#### Scenario: 请求航线任务资源
- **WHEN** 显式请求 flight_id="FLT-001" 的航线资源
- **THEN** 模拟器发布 method=`flighttask_resource_get`、data=`{flight_id:"FLT-001"}` 的 requests 报文

#### Scenario: 航线准备触发资源请求
- **WHEN** `flighttask_prepare` 指令携带 flight_id="FLT-001" 且飞控处理成功
- **THEN** 模拟器正常回发 services_reply，并自动发布该 flight_id 的 `flighttask_resource_get`

#### Scenario: 请求飞行区域与离线地图
- **WHEN** 显式请求飞行区域或离线地图资源
- **THEN** 模拟器分别发布 method=`flight_areas_get` 或 `offline_map_get` 且 data 为空对象的 requests 报文

#### Scenario: 航线任务标识非法
- **WHEN** 航线资源请求的 flight_id 为空
- **THEN** 模拟器不发布请求、不创建 pending，并返回空 bid

### Requirement: 航线资源响应状态
UAVMqttBridge MUST 将匹配的 `flighttask_resource_get` 成功响应按 `data={result:0,output:{file:{url,fingerprint}}}` 解析，并把请求中的 flight_id 与非空 url / fingerprint 原子记录为最新航线资源状态。result 非 0、output/file 缺失或字段为空时 MUST 将请求完成结果标记为失败且不得覆盖最近一次成功状态。

#### Scenario: 航线资源响应成功
- **WHEN** 收到匹配响应且 output.file 含非空 url 与 fingerprint
- **THEN** 模拟器记录请求对应的 flight_id、url、fingerprint 并广播请求成功

#### Scenario: 航线资源响应失败
- **WHEN** 收到匹配响应但 result 非 0，或 output.file 缺少 url / fingerprint
- **THEN** 模拟器广播请求失败并保留最近一次成功的航线资源状态

### Requirement: 飞行区域资源响应状态
UAVMqttBridge MUST 将匹配的 `flight_areas_get` 成功响应按 `data={result:0,output:{files:[...]}}` 解析。files MUST 存在且允许为空；每个文件 MUST 含符合 `geofence_<32位字母数字>.json` 格式的 name、非空 url / checksum 和非负整数 size。全部文件有效时 MUST 原子更新最近飞行区域状态；任一文件非法或响应失败时 MUST 保留最近一次成功状态。

#### Scenario: 飞行区域文件列表成功
- **WHEN** 响应 files 含一个字段完整且名称合法的飞行区域文件
- **THEN** 模拟器记录 name / url / checksum / size 并广播请求成功

#### Scenario: 飞行区域空列表成功
- **WHEN** 响应 result=0 且 output.files 为空数组
- **THEN** 模拟器记录有效的空飞行区域列表并广播请求成功

#### Scenario: 飞行区域文件非法
- **WHEN** 任一文件名称格式非法、字段缺失或 size 不是非负整数
- **THEN** 模拟器广播请求失败且不覆盖最近一次成功状态

### Requirement: 离线地图资源响应状态
UAVMqttBridge MUST 将匹配的 `offline_map_get` 成功响应按 `data={result:0,output:{offline_map_enable,files:[...]}}` 解析。offline_map_enable MUST 为布尔值且 files MUST 存在并允许为空；每个文件 MUST 含符合 `offline_map_full_<非空字母数字或下划线>.rocksdb.zip` 格式的 name、非空 url / checksum 和非负整数 size。全部字段有效时 MUST 原子更新最近离线地图状态；响应失败或字段非法时 MUST 保留最近一次成功状态。

#### Scenario: 离线地图启用并返回文件
- **WHEN** 响应 offline_map_enable=true 且 files 全部合法
- **THEN** 模拟器记录启用状态及文件元数据并广播请求成功

#### Scenario: 离线地图关闭且无文件
- **WHEN** 响应 offline_map_enable=false 且 files 为空数组
- **THEN** 模拟器记录离线地图关闭状态并广播请求成功

#### Scenario: 离线地图响应非法
- **WHEN** offline_map_enable 缺失/类型错误，或任一文件字段非法
- **THEN** 模拟器广播请求失败且不覆盖最近一次成功状态

### Requirement: 资源更新指令与同步进度
UAVMqttBridge MUST 精确处理 services method `flight_areas_update` 与 `offline_map_update`。收到任一指令时 MUST 回发 `data={result:0,output:{status:"sent"}}` 并触发对应资源请求。由更新指令触发的资源响应成功后 MUST 发布同名同步进度事件 `flight_areas_sync_progress` 或 `offline_map_sync_progress`，data 含 `status="synchronized"`、`reason=0`，非空文件列表时 file 含第一项的 name / checksum；解析失败或请求超时则 MUST 发布 `status="fail"`、`reason=1`。

#### Scenario: 飞行区域更新成功
- **WHEN** 收到 flight_areas_update 且随后 flight_areas_get 返回有效文件列表
- **THEN** 模拟器回发 sent、发布资源请求，并发布 flight_areas_sync_progress synchronized 事件

#### Scenario: 离线地图更新成功
- **WHEN** 收到 offline_map_update 且随后 offline_map_get 返回有效响应
- **THEN** 模拟器回发 sent、发布资源请求，并发布 offline_map_sync_progress synchronized 事件

#### Scenario: 更新资源解析失败
- **WHEN** 更新指令触发的资源响应 result 非 0 或字段非法
- **THEN** 模拟器发布对应 sync_progress fail 事件，reason=1

### Requirement: 主动请求超时清理
UAVMqttBridge MUST 为 pending 主动请求记录创建时间，并按可配置的正数超时时间清理未响应请求。超时请求 MUST 从 pending 移除并广播请求失败；若请求由飞行区域或离线地图更新指令触发，还 MUST 发布对应 sync_progress fail 事件。断连清理保持现有行为，不得在超时或断连时覆盖任何最近成功的资源状态。

#### Scenario: 普通资源请求超时
- **WHEN** pending 请求等待时间超过配置阈值且未收到匹配响应
- **THEN** 模拟器移除该请求并广播请求失败

#### Scenario: 更新资源请求超时
- **WHEN** flight_areas_update 或 offline_map_update 触发的请求超过配置阈值
- **THEN** 模拟器移除该请求、广播失败并发布对应 sync_progress fail 事件
