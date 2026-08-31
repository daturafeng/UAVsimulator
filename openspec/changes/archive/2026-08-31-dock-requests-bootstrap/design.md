## Context

UUAVMqttBridgeComponent 已集中管理 MQTT 连接、订阅与所有上云 API 报文发布，但现有数据流以“云端下发、设备回执”为主，没有设备主动请求和异步响应关联。dock 的 requests 成功响应存在两种 data 形态：config 直接返回 ProductConfigResponse，其余本变更方法返回 MqttReply<T>（{result, output}）。详见 proposal.md - Why 与 specs/mqtt-bridge/spec.md。

## Goals / Non-Goals

**Goals:**
- 在现有桥接组件内形成可复用的主动 request 发布、待处理关联与 requests_reply 分发能力。
- 让上线配置与组织绑定握手按响应顺序推进，避免无绑定码时持续发送必然失败的请求。
- 对成功响应保留可查询状态，对失败/错配响应保持旧状态并广播结果，便于蓝图或后续业务消费。

**Non-Goals:**
- 不实现 flighttask_resource_get / flight_areas_get / offline_map_get，分别留给航线资源、空域和离线地图变更。
- 不使用获取到的 OSS 凭证执行真实文件上传，也不把临时凭证持久化到磁盘。
- 不增加定时重试、超时重发或跨进程恢复；MQTT 重连重新启动握手。
- 不新增组织选择 UI；绑定码、组织 ID 与设备呼号使用组件配置。

## Decisions

1. **统一 request 报文工具放在 UAVCore**：新增 MakeRequestMessage(method, gateway, data, tid, bid)，与 MakeEventMessage / MakeServicesReply 同层，统一生成 tid / bid / timestamp。备选是在桥接组件内手工拼 JSON，但会重复协议头逻辑。
2. **待处理请求按 bid 索引并校验 tid + method**：桥接组件保存 FPendingCloudRequest{Tid, Method}，requests_reply 命中 bid 后仍要求 tid 和 method 完全一致才消费。仅按 bid 关联实现更简单，但不能满足 cloud-sdk 对 tid / bid 同时匹配的约束。
3. **启动握手分阶段发布**：连接成功且 requests_reply 订阅后立即发布 config 与 airport_bind_status；只有绑定状态确认未绑定、且绑定码非空时才请求组织信息，组织查询成功后再发布绑定。相比连接后一次性发布全部请求，分阶段能避免未配置或已绑定设备产生无意义错误。
4. **按 method 使用两类响应解析器**：config 直接解析 data 产品配置；organization / storage 先校验 data.result=0，再解析 output。所有解析器先构建临时状态，字段完整后一次性替换最近成功状态，避免半更新。
5. **重连清理瞬时关联、保留最近成功状态**：断连时清空 pending requests；产品配置、组织信息与存储配置继续保留，重连响应成功后覆盖。这样不会让旧 bid 在新会话被误匹配，也避免 UI 在短暂断线时丢失最后已知信息。
6. **凭证不写日志**：storage_config_get 仅记录结构化配置供运行时使用，日志只记录 method / bid / result，不序列化 credentials 内容。

## Risks / Trade-offs

- [dock 未配置绑定码或组织不存在] → 停在未绑定状态并广播失败，不自动循环重试；用户修正配置后通过重连或显式请求重新触发。
- [连接回调内连续发布两个请求] → 先完成 requests_reply 订阅，再发布启动请求；每个请求使用独立 tid / bid。
- [临时 OSS 凭证会过期] → 本变更只保存最近响应；后续真实上传流程在每次上传前显式刷新 storage_config_get。
- [响应字段版本差异] → 只读取本变更依赖字段并忽略额外字段；必填字段缺失时不覆盖旧状态。
