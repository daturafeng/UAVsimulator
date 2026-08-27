## Why

已归档的 rtmp-live-streaming 实现了真实 RTMP 推流，但设计明确把"推流失败自动重连"列为 Non-Goal：FFmpeg 子进程意外退出（如 RTMP 服务器未就绪、网络抖动、推流地址短暂不可达）时，组件立即把会话标记为停止并回收进程，推流不会自行恢复。dock 侧 ZLMediaKit/SRS 可能比模拟器晚就绪，或联调中临时重启，一次失败就导致直播能力中断，需要重新下发 live_start_push 才能恢复。

## What Changes

- UAVCameraStream 增加推流失败自动重连：FFmpeg 意外退出时按可配置策略（次数上限、间隔、退避）自动重新启动推流，重连成功则推流恢复，达到上限才停止会话并广播 live_status。
- 重连期间保持会话推流状态语义：bStreaming 在重连等待期间不置 false（对上层与 dock 表现为推流会话持续存在），仅在重连耗尽后结束；live_stop_push 在重连等待期间可立即取消重连并停止。
- 重连策略参数化（EditAnywhere）：MaxReconnectAttempts（默认 3，0 表示不重连，保持原行为）、ReconnectIntervalSeconds（默认 5 秒）、ReconnectMaxIntervalSeconds（退避上限，默认 30 秒）。
- 重连策略（是否继续重试、下次延迟）提取为纯函数并新增自动化测试；Timer 调度放在组件内，EndPlay/销毁时清理定时器，不产生悬挂回调。
- 严格模式（bRequireFfmpeg）下启动即失败的行为不变；重连仅作用于"已成功启动后意外退出"的场景。

## Capabilities

### New Capabilities
<!-- 无：camera-streaming 能力已存在，本次为需求增强 -->

### Modified Capabilities
- camera-streaming: 真实 RTMP 推流在 FFmpeg 意外退出时自动重连，重连策略可配置且可测试。

## Impact

- Source/UAVCameraStream：UAVCameraStreamComponent 增加重连调度（TimerManager）与配置项；UAVFfmpegCommand 增加重连策略纯函数。
- Source/UAVCameraStream/Private/Tests：新增重连策略自动化测试。
- 行为变化：FFmpeg 意外退出时不再立即停止会话，而是按策略重连（默认最多 3 次、5 秒间隔、退避）；重连耗尽后行为与原先一致（停止 + live_status 事件）。live_stop_push 与 EndPlay 的兜底清理不受影响。
