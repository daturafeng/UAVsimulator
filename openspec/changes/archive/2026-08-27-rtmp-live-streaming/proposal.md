## Why

UAVCameraStream 目前只有直播会话模型（live_start_push 仅记录 video_id/RTMP 地址/清晰度），不会真正把画面推给 dock。项目定位要求"把相机画面通过 RTMP 推送给 dock"，dock 侧用 ZLMediaKit/SRS 收流；没有真实推流，dock 的直播能力无法联调。

## What Changes

- UAVCameraStream 增加真实 RTMP 推流管线：
  - 画面源：配置 UTextureRenderTarget2D（由 SceneCapture2D 或其他方式生成相机画面），未配置时自动在场景创建 SceneCapture2D 指向目标。
  - 编码推流：按配置帧率从 RenderTarget 读取 BGRA 像素，写入 FFmpeg 子进程 stdin，由 FFmpeg 编码 H.264 并以 FLV 推送到会话的 RTMP 地址。
  - 清晰度映射：live_set_quality 的 0-4 档位映射为分辨率/码率/帧率参数（与上云 API video_quality 语义一致）。
  - 生命周期：live_start_push 启动管线、live_stop_push 停止并回收子进程；组件销毁/EndPlay 时兜底清理。
  - FFmpeg 定位：路径可配置（默认探测 PATH 与常见安装位置）；缺失时降级为会话模型占位（保持现有行为，不崩溃），可通过配置要求严格模式返回非 0。
- 保持可测：FFmpeg 命令构造、清晰度档位映射、video_id 解析等为纯函数，可单元测试。
- 不引入引擎插件依赖（NDI/PixelStreaming 均非 RTMP 输出）；FFmpeg 为可选外部工具，作为既有能力缺失时的本地实现，边界与替代方案见 design.md。

## Capabilities

### New Capabilities
- `camera-streaming`: 相机载荷的直播会话与真实 RTMP 推流：画面捕获、FFmpeg 编码推流、清晰度档位、推流生命周期。

### Modified Capabilities
<!-- 无：mqtt-bridge 协议行为不变，仅消费 live_status 事件 -->

## Impact

- `Source/UAVCameraStream`：UAVCameraStreamComponent 增加推流管线与 FFmpeg 子进程管理；新增画面捕获辅助类（或组件内实现）。
- `Source/UAVCameraStream/UAVCameraStream.Build.cs`：新增 RenderCore/RHI 等像素读取所需模块依赖。
- `Source/UAVCore`（可选）：新增视频档位映射工具常量/函数，便于测试与复用。
- 外部依赖：FFmpeg 可执行文件（可选，路径可配置；缺失时降级）。
- 行为变化：live_start_push/live_stop_push/live_set_quality 从"仅维护会话"变为"驱动真实推流"，事件与 MQTT 桥接层无需改动。
