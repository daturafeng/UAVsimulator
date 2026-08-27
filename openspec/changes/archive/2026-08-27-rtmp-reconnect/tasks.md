## 1. 重连策略纯函数（UAVFfmpegCommand）

- [x] 1.1 UAVFfmpegCommand 新增 ShouldRetryReconnect(AttemptsUsed, MaxAttempts) 与 NextReconnectDelaySeconds(Attempt, Base, Max) 纯函数（退避指数、封顶、禁用语义）
- [x] 1.2 新增重连策略自动化测试：禁用重连、未达上限继续、达到上限停止、退避递增与封顶

## 2. 组件重连调度（UAVCameraStream）

- [x] 2.1 UAVCameraStreamComponent 增加配置：MaxReconnectAttempts / ReconnectIntervalSeconds / ReconnectMaxIntervalSeconds
- [x] 2.2 FFmpeg 意外退出路径改为：回收进程与管道 → 调度重连（记录 video_id 与已用次数），而非直接停止会话
- [x] 2.3 重连回调执行 StartStreaming：成功清空重连状态；失败按策略继续或达到上限停止会话并广播 live_status
- [x] 2.4 live_stop_push / live_set_quality / EndPlay 取消重连 Timer 并清空状态，防止旧回调复活推流

## 3. 验证与归档

- [x] 3.1 编译通过（UE 5.7 UBT 构建 UAVsimulatorEditor）
- [x] 3.2 自动化测试通过（重连策略单测）
- [x] 3.3 openspec validate 校验变更
