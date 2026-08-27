## 1. 机场 OSD 维护状态字段

- [x] 1.1 BuildDockOsdPayload 在 activation_time 后输出 maintain_status（maintain_status_array 单元素：last_maintain_flight_sorties=0 / last_maintain_time=0 / last_maintain_type=0 / state=false），中文注释

## 2. 自动化测试

- [x] 2.1 UAVDockOsdTests.Structure：RequiredFields 增加 maintain_status，并断言 maintain_status_array 数组结构（首元素含四字段）

## 3. 验证与归档

- [x] 3.1 编译通过（UE 5.7 UBT 构建 UAVsimulatorEditor）
- [x] 3.2 自动化测试通过（UAV.MqttBridge.DockOsd 新增断言 + UAV.* 全量回归，19 项全绿）
- [x] 3.3 openspec validate 校验变更并归档；git commit + push
