# camera-streaming Specification

## Purpose

相机载荷的直播会话与真实 RTMP 推流能力：将无人机相机画面编码为 H.264 并通过 RTMP 推送给 dock 的流媒体服务器，支持清晰度档位切换、推流生命周期管理与推流失败自动重连，供上云 API 直播指令（live_start_push / live_stop_push / live_set_quality / live_lens_change）驱动。

## ADDED Requirements

### Requirement: 推流失败自动重连
UAVCameraStream MUST 在 FFmpeg 推流子进程意外退出时按可配置策略自动重新启动推流：重连成功则恢复推流；达到重连次数上限后才停止会话并广播状态事件。重连策略（次数上限、基础间隔、退避上限）可配置，策略计算可脱离 FFmpeg 单独校验。

#### Scenario: 意外退出触发重连
- **WHEN** FFmpeg 推流子进程意外退出（如 RTMP 服务器不可达）且重连未耗尽
- **THEN** 组件回收子进程与管道，按退避间隔重新启动 FFmpeg 推流，重连期间会话保持推流状态

#### Scenario: 重连耗尽停止
- **WHEN** FFmpeg 重连次数达到上限仍无法建立推流
- **THEN** 会话标记为停止推流，广播 live_status 事件，并回收子进程资源

#### Scenario: 重连可取消
- **WHEN** 重连等待期间收到 live_stop_push 或组件销毁
- **THEN** 取消重连调度并停止会话，不产生悬挂定时器或悬挂进程

#### Scenario: 重连策略可配置
- **WHEN** 重连次数上限为 0
- **THEN** 不启用自动重连，FFmpeg 意外退出时按原有行为立即停止会话
