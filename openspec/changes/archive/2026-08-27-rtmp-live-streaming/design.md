## Context

UAVCameraStream 现有实现为纯会话模型（见已归档变更 mqtt-bridge-integration 的 live_* 指令处理）：live_start_push 仅记录 video_id/RTMP 地址/清晰度，live_stop_push 仅置 bStreaming=false，不产生真实画面。引擎媒体插件（NDI/PixelStreaming/MediaFramework 等）均不提供 RTMP 编码推流输出；dock 侧通过 ZLMediaKit/SRS 接收 RTMP 流。因此需引入 FFmpeg 子进程作为编码推流器：UE 侧读取 RenderTarget 像素（BGRA），写入 FFmpeg stdin（rawvideo），FFmpeg 编码 H.264 并以 FLV 推送。

## Goals / Non-Goals

**Goals:**
- 在既有 live_* 指令语义上叠加真实推流：会话模型不变，推流管线作为会话状态机的执行层。
- 推流参数（分辨率/码率/帧率）由清晰度档位（0-4）映射，映射为纯函数可单测。
- FFmpeg 缺失时降级不崩溃，保持现有 MQTT 桥接与事件行为不变。
- 组件生命周期内（BeginPlay/EndPlay/销毁）推流资源可回收。

**Non-Goals:**
- 不做音频采集与混流（首期仅视频；FFmpeg 命令不含音频输入）。
- 不做多路并发会话同时推流（首期单会话推流，多会话仅维护模型；设计中留扩展点）。
- 不做推流失败自动重连/码率自适应（首期失败即停止并记录日志，由上层决定重试）。
- 不引入引擎插件或第三方 UE 库（NDI/PixelStreaming 等）；FFmpeg 为可选外部可执行文件。

## Decisions

- **画面源为 UTextureRenderTarget2D + SceneCapture2D**：组件暴露 RenderTarget 配置（EditAnywhere）；未配置时在 BeginPlay 自动创建 SceneCaptureComponent2D 挂到无人机 Actor，渲染到新建 RenderTarget。备选（直接读主相机 backbuffer）跨平台/多 PIE 不稳定，且依赖视口存在，弃用。
- **FFmpeg 子进程 stdin 管道喂帧**：命令模板 `ffmpeg -f rawvideo -pix_fmt bgra -s {W}x{H} -r {fps} -i - -c:v libx264 -preset ultrafast -tune zerolatency -b:v {kbps}k -f flv {rtmpUrl}`。备选：把帧写成磁盘临时文件再推（IO 大、延迟高）；用 UE MediaFramework 输出（不支持 RTMP）。
- **帧读取节流在游戏线程 Tick**：按档位帧率（默认 15fps）节流，从 RenderTarget 同步 ReadPixels（BGRA）后写入子进程 stdin。备选：RHI 异步读回 + 后台写线程（复杂度高，首期不需要）。注意 ReadPixels 在游戏线程有开销，档位分辨率上限控制（超清不超过 1920x1080@15）。
- **FFmpeg 定位与降级策略**：FfmpegPath 可配置；为空时依次探测 PATH（`ffmpeg`）与常见安装目录。探测失败且严格模式（bRequireFfmpeg=true）→ live_start_push 返回非 0；否则进入占位推流（bStreaming=true 但无子进程）并 Warning 日志。
- **清晰度档位映射放 UAVCore 工具命名空间**：`UAV::CloudApi` 提供 `GetVideoQualityParams(int32)` 纯函数返回 {Width,Height,BitrateKbps,Fps}，便于 UAVCameraStream 与单元测试共用。
- **进程生命周期集中管理**：推流管线在组件内以 RAII 风格封装（Start/Stop/Tick 写帧/EndPlay 清理），子进程句柄与 stdin 管道句柄在 Stop 时关闭；FFmpeg 退出时检测退出码并日志。
- **保持 MQTT 桥接零改动**：live_status 事件语义不变（video_id 参数），桥接层继续消费 OnLiveStatusChanged 转发 events；推流管线仅影响 UAVCameraStream 内部。

## Risks / Trade-offs

- [ReadPixels 游戏线程开销] → 档位分辨率/帧率上限控制；首期默认流畅档 640x360@15fps；若实测卡顿可后续切 RHI 异步读回。
- [FFmpeg 缺失或路径错误] → 探测失败降级为占位推流 + Warning；严格模式返回非 0；文档注明需安装 FFmpeg 并配置路径。
- [RTMP 服务器不可达/推流失败] → FFmpeg 子进程非零退出时停止会话推流状态并日志；不自动重连（非目标）。
- [渲染目标未更新（无场景/未加载地图）] → 推流启动时校验 RenderTarget 有效性与尺寸，无效则视为占位推流并警告。

## Migration Plan

无存量数据迁移。行为兼容：未配置画面源/无 FFmpeg 时，既有 live_* 会话行为与 MQTT 事件保持不变；配置后自动启用真实推流。

## Open Questions

无。
