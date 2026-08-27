## Context

UAVCameraStream 的推流管线由 FFmpeg 子进程承载：组件按会话清晰度档位帧率节流读取 RenderTarget 像素并写入子进程 stdin。当前 TickComponent 检测到 FFmpeg 非预期退出（GetProcReturnCode 有返回值）时，立即置 bStreaming=false、回收进程与管道并广播 live_status。RTMP 服务器不可达、服务重启、网络抖动都会触发该路径，推流一旦中断无法自行恢复。

## Goals / Non-Goals

**Goals:**
- FFmpeg 意外退出时按可配置策略自动重连，重连成功则推流恢复，避免 dock 侧需要重新下发指令。
- 重连等待期间 live_stop_push / EndPlay 可立即取消重连并停止，不产生悬挂进程或悬挂 Timer 回调。
- 重连策略（次数上限、间隔、退避）参数化且可单测。
- 保持既有会话模型、事件语义与 MQTT 桥接层零改动。

**Non-Goals:**
- 不做码率自适应（码率仍由清晰度档位决定；重连沿用会话当前档位参数）。
- 不做多路并发会话同时重连（首期单会话真实推流，重连仅作用于当前活动推流）。
- 不做推流启动前的预探测与排队（重连即按当前档位直接重新启动 FFmpeg）。
- 不修改 live_start_push 的严格模式语义（启动即失败仍返回非 0 并回滚会话）。

## Decisions

- **重连状态模型**：重连是推流管线的内部行为，不改变会话状态机。会话 bStreaming 在重连等待与重试期间保持 true（对上层/dock 表现为推流会话持续存在）；达到重连上限后才置 false 并广播 live_status。这与"推流失败即停止"的旧行为在事件层面兼容：用户仍会收到一次 live_status 停止事件，只是默认延迟到重连耗尽后。
- **策略参数**：MaxReconnectAttempts（0=禁用重连，保持旧行为；默认 3）、ReconnectIntervalSeconds（基础间隔，默认 5）、ReconnectMaxIntervalSeconds（退避上限，默认 30）。退避采用指数：delay = min(Base * 2^attempt, Max)，attempt 从 0 起。
- **重连策略为纯函数**：ShouldRetryReconnect(int32 AttemptsUsed, int32 MaxAttempts) 与 NextReconnectDelaySeconds(int32 Attempt, double BaseSeconds, double MaxSeconds) 放入 UAVFfmpegCommand，便于单元测试与复用。组件只负责用 TimerManager 调度。
- **调度方式**：意外退出时清除旧进程/管道，记录重连目标 video_id 与已用次数，通过 GetWorld()->GetTimerManager() 设置单次 Timer；到点回调执行 StartStreaming（StartStreaming 内部已有"先停旧推流"的幂等逻辑），成功则清空重连状态，失败则次数+1 并再次调度（或达到上限停止）。不依赖 Tick 轮询，重连等待期间组件可休眠。
- **取消路径**：HandleLiveStopPush、HandleLiveSetQuality（重启推流）、EndPlay 均先 InvalidateTimer 并清空重连状态，再走原有停止逻辑，避免旧 Timer 复活已停止的推流。
- **严格模式不变**：StartStreaming 首次启动失败（FFmpeg 不可用）仍由调用方决定回滚；重连仅在"启动成功后进程退出"的路径触发，因此严格模式仅影响首次启动。
- **测试范围**：纯函数单测覆盖（禁用重连、达到上限、退避封顶、间隔计算）；组件级 Timer/重连调度依赖引擎 Timer 与真实 FFmpeg，不做自动化集成测试。

## Risks / Trade-offs

- [重连期间无真实画面] → 重连等待/重试期间 RTMP 服务器侧无流（与旧行为一致，旧行为直接永久停止）；通过日志区分"重连中"与"已停止"。
- [无限重连拖住会话] → 默认上限 3 次 + 退避封顶，耗尽后即停止并广播，不会无限占用。
- [Timer 在 PIE/关卡切换时残留] → EndPlay 统一 InvalidateTimer；组件销毁由引擎保证 EndPlay 调用。
- [重连期间收到 live_set_quality] → 视为取消重连并立即按新档位重启推流（现有 restart 逻辑），语义清晰。

## Migration Plan

无存量数据迁移。默认 MaxReconnectAttempts=3 改变了"一次失败即停止"的旧行为；若需严格保持旧行为，配置 MaxReconnectAttempts=0 即可。MQTT 桥接层与事件模型零改动。

## Open Questions

无。
