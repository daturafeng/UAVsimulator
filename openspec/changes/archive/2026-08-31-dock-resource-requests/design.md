## Context

现有 UAVMqttBridge 已订阅 requests_reply，使用 bid 索引并校验 tid / method，且能在响应成功时原子更新配置和绑定状态。三类资源请求应继续使用该框架，不能建立第二套 MQTT 相关机制。dock 的三个响应均为 `MqttReply<T>`；其中离线地图在当前 sample 中没有 concrete override，因此无响应超时是必须覆盖的运行时路径。

## Goals / Non-Goals

**Goals:**

- 在一个请求关联框架内覆盖 dock 的全部八种 RequestsMethodEnum。
- 保存足以支持后续下载/解析的资源元数据，并对格式错误实施原子状态保护。
- 将云端飞行区域/离线地图更新指令闭环到资源请求与同步结果事件。
- 避免无响应请求无限留在 pending 中。

**Non-Goals:**

- 不下载、校验实际字节内容、解压或解析 KMZ、JSON、RocksDB 文件。
- 不让飞行区域或离线地图内容参与本地飞行限制。
- 不重试请求；超时后由后续服务指令或显式 API 再次触发。
- 不改变 FlightControl 的航点注入和默认演示航线策略。

## Decisions

### 1. 扩展现有 pending 记录而非新增资源请求器

pending 记录增加创建时间、flight_id 上下文和“是否需要同步进度”标志。响应分发在移除记录前复制上下文，并将其传给资源解析器。这样 tid / bid / method 的安全边界保持唯一，航线响应也能恢复请求中才存在的 flight_id。

备选方案是按资源类型建立三张 pending 表；它会重复关联与断连清理逻辑，因此不采用。

### 2. 状态使用 Blueprint 可读结构并采用临时值解析

航线资源、通用资源文件、飞行区域列表和离线地图列表使用 USTRUCT 暴露只读状态。响应先完整解析到局部临时结构，所有字段通过后一次性赋值；失败路径不触碰旧状态。

文件名校验使用确定性的前缀/后缀/字符检查，避免为两个固定协议格式引入额外正则依赖。

### 3. 更新指令只模拟元数据同步

`flight_areas_update` / `offline_map_update` 立即回发 sent，再以 pending 标志区分显式请求和更新流程请求。有效响应发布 synchronized，失败或超时发布 fail。file 仅回报第一项 name / checksum，空列表时省略 file，和 DTO 的可空字段保持一致。

### 4. Tick 统一执行超时清理

每次 Tick 使用单调时钟检查 pending，配置项 `RequestTimeoutSeconds` 必须为正数。清理先收集 bid 再移除，避免遍历期间修改容器；每个超时请求仅广播一次失败。断连仍直接清空 pending，不额外发布同步事件，防止网络切换期间产生误报。

### 5. 航线准备保持现有飞控时序

`flighttask_prepare` 仍先由 FlightControl 校验和处理，成功后异步发起航线资源请求。此变更只记录 URL 与 fingerprint，不延迟既有 flighttask_ready；等后续引入 KMZ 下载和解析时，再通过独立 OpenSpec 变更调整就绪时序。

## Risks / Trade-offs

- [flighttask_ready 早于实际资源下载] → 本变更明确只同步元数据；后续航线消费变更将负责延迟就绪。
- [dock sample 不实现 offline_map_get] → pending 超时后清理并发布 fail，不阻塞其他请求。
- [URL 或 checksum 语义有效但资源不可访问] → 当前只校验结构，真实网络与内容校验留给下载层。
- [Tick 中 pending 数量增长] → 设备请求规模很小且有超时上限，线性扫描成本可忽略。

## Migration Plan

1. 先发布协议常量、状态结构和响应解析，保持现有请求行为兼容。
2. 启用 update services 分发与超时清理。
3. 通过协议、解析、失败保护、超时、完整 UAV 自动化测试后同步主规格并归档。
4. 回滚时整体回退本变更；既有五类 requests 和 services 行为不受影响。
