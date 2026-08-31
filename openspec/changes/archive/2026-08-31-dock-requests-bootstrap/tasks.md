## 1. 协议基础

- [x] 1.1 UAVCore：新增 requests / requests_reply topic 模板与 config / airport_bind_status / airport_organization_get / airport_organization_bind / storage_config_get method 常量
- [x] 1.2 UAVCore：新增 MakeRequestMessage，统一生成 tid / bid / timestamp / gateway / method / data

## 2. 请求发布与关联

- [x] 2.1 UAVMqttBridge：新增 requests_reply 订阅、PublishRequest、pending request（bid 索引，tid / method 精确校验）与断连清理
- [x] 2.2 UAVMqttBridge：新增 config / airport_bind_status / airport_organization_get / airport_organization_bind / storage_config_get 请求 data 构建入口
- [x] 2.3 UAVMqttBridge：连接成功且订阅完成后发布 config 与 airport_bind_status；提供显式 storage_config_get 发布入口

## 3. 响应解析与组织握手

- [x] 3.1 UAVMqttBridge：解析 config 直接响应，字段完整时原子更新产品配置并广播完成结果
- [x] 3.2 UAVMqttBridge：解析 airport_bind_status MqttReply；未绑定且绑定码已配置时触发 airport_organization_get
- [x] 3.3 UAVMqttBridge：解析 airport_organization_get 后触发 airport_organization_bind，解析 err_infos 并更新机场/无人机绑定状态
- [x] 3.4 UAVMqttBridge：解析 storage_config_get，成功时原子更新运行时对象存储配置，失败时保留最近成功状态
- [x] 3.5 UAVMqttBridge：未知 bid 或 tid / method 错配响应不消费 pending、不更新状态

## 4. 自动化测试

- [x] 4.1 UAVCore/Tests：断言 requests topic 常量与 MakeRequestMessage 报文头、data 透传
- [x] 4.2 UAVMqttBridge/Tests：断言五类 request data 与上线启动请求结构
- [x] 4.3 UAVMqttBridge/Tests：断言 config / bind_status / organization_get / organization_bind / storage_config_get 成功与失败解析
- [x] 4.4 UAVMqttBridge/Tests：断言 tid / bid / method 关联、未知响应保护与失败响应不覆盖旧状态

## 5. 构建与交付

- [x] 5.1 UBT 构建 UAVsimulatorEditor（Win64 Development）
- [x] 5.2 Automation RunTests UAV 全部通过（含新增用例与既有基线）
- [x] 5.3 openspec validate --specs 通过，按流程同步规格、归档、git commit + push
