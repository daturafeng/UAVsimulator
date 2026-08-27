## 1. 清晰度档位工具（UAVCore）

- [x] 1.1 UAVCloudApiTypes 新增 UAV::CloudApi::FUAVVideoQualityParams + GetVideoQualityParams(int32) 返回 {Width,Height,BitrateKbps,Fps}，0-4 档位映射（自适应默认 1280x720@15/2000kbps）
- [x] 1.2 新增 UAVVideoQualityTests 自动化测试：0-4 档位参数确定性、非法档位回退自适应

## 2. 推流管线（UAVCameraStream）

- [x] 2.1 UAVCameraStreamComponent 增加推流配置：RenderTarget（EditAnywhere）、FfmpegPath、bRequireFfmpeg、默认帧率
- [x] 2.2 BeginPlay 自动创建 SceneCapture2D + RenderTarget（未配置时），挂到无人机 Actor 并指向目标
- [x] 2.3 新增 FFmpeg 子进程封装（启动/停止/写帧/退出检测），命令按档位参数构造
- [x] 2.4 会话状态机接入推流：live_start_push 启动推流、live_stop_push 停止、live_set_quality 运行时切换参数
- [x] 2.5 EndPlay/销毁兜底停止推流并回收子进程句柄与管道

## 3. 降级与可测性

- [x] 3.1 FFmpeg 探测逻辑（PATH + 常见安装目录）；缺失时降级为占位推流并 Warning，严格模式返回非 0
- [x] 3.2 推流命令构造为纯函数并新增自动化测试（含 URL 转义、档位参数注入）
- [x] 3.3 编译通过（UE 5.7 UBT 构建 UAVsimulatorEditor）

## 4. 验证与归档

- [x] 4.1 通过 openspec validate 校验变更
- [x] 4.2 同步主 spec 并归档变更
