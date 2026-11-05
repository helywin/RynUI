# Upload Linux 验收

> 本文件只记录源分支 `t3/native-image-upload`（最终 `217a5ea0`）的验证；不作为合并后 main 的通过证据。主线结果见 [Image/Upload 集成验收](image-upload-integration.md)。

- Linux x64 / GCC / Ninja Multi-Config，`linux-gcc-headless` Debug、Release 最终相关 CTest 各 **62/62**；公共 Upload / NativeFileServices / Form mapping 与文档示例 compile targets 均构建通过。完整 targets、regex 与共享图像/GPU 命令见 [Image 结果](image-linux.md)。Core 不 include/link renderer、SDL、系统字体或平台 IO。
- 包含 **14 Upload component / 10 Upload model / 18 Image component**：稳定 UID/重排、受控拒绝/删除、accept/multiple/maxCount、真实 beforeUpload Ignore/transform、独立进度/终态/retry/abort、旧/重复/外线程 completion、chooser 后续打开/退休、window/region/input-boundary drop、布局更新后命中、paste、真实圆形缩略图、保留 rich item 草稿、renderer 局部替换、throw/self-destroy、rich 输入/IME 清理、idle 零帧/资源退休。Form UID 不变的 metadata/progress 也实际更新；预览按 UID 保留变换，locale/RTL 交给共享 PreviewGroup。
- `linux-gcc` Debug SDK/输入测试 **4/4**；SDK chooser seam **26 checks**，覆盖外线程 SDK 回调复制、owner drain、workspace fixture IO、目录、取消、晚到旧 owner/后续打开、错误及输入上限。Drop.data 按锁定 SDL3 临时所有权复制、不自行释放；ordered Shift/Ctrl/Meta 兼容保留。

```text
cmake --build --preset linux-gcc-debug --target rynui_file_service_tests rynui_sdl_event_adapter_tests rynui_platform_input_tests rynui_platform_event_pump_tests -j2
ctest --preset linux-gcc-debug -R '^rynui\.(file_service_sdk|sdl_event_adapter|platform_input|platform_event_pump)$' --output-on-failure
out/build/linux-gcc/tests/Debug/rynui_file_service_tests
python tools/run_background_wayland.py --log-dir out/upload-native-file-clipboard --timeout 30 -- out/build/linux-gcc/tests/Debug/rynui_file_service_tests --clipboard
```

- 最后一条为私有 Wayland 下 SDL 同应用 `text/uri-list` 百分号解码并读取实际 PNG fixture，**7/7 checks**。没有调用 OS chooser；26 SDK seam 与 7 同应用 clipboard checks **不代表真实 OS chooser/portal 或跨应用 clipboard 验收**。
- 独立 Upload Gallery 使用真实 fixture、native Upload/Image、稳定受控列表，清楚标注**合成应用 transport**。按钮显式交付 progress/success/failure 与异步 remove，有实际 Ignore/retry/abort/late-success 拒绝、四种列表及 retained draft；无默认 HTTP 或伪造服务器响应。
- 最终私有 Mutter Wayland / Vulkan / SPIR-V 页面验收 **2 cases / 247 checks / 50 submitted frames，passed**：实际半透明 Image 缩略图读回、Ignore/progress/failure/retry/success/cancel-stale、默认预览、富输入草稿保留、异步移除和整页退休。共享图像独立 GPU 检查 **49/49**，不是 Recording 证据。支持 overlay 仅在实际原生验证后标记 implemented。
- 当前 generator **89 samples**，基线 **87 retained / 0 removed / 2 added**，legacy IDs **458**；既有 Theme **61** 字段除 identity 保留。父任务后续主线样例/Theme/Form 增量由整合任务合并；无 dirty rebase/reset。格式 **961/0**，doctor healthy、strict **107/107**、catalog/source checks 与 diff/check 通过。
- SDL chooser 已实现 SDK adapter，但没有 OS request 关闭 API，取消先使结果失效，退休回调只清理。默认宿主只为 PNG/JPEG/BMP 建立图片 bytes/MIME（单文件 16 MiB、总计 64 MiB、4096 项），其他文件保留 locator/metadata；目录不递归跟随 symlink。实际 OS chooser/portal、跨应用 clipboard、Windows/MSVC/D3D12、系统 AT/IME **未运行**。不打开用户桌面 dialog、不读取私人文件，所有原始日志/图片/源码副本只在 ignored out。
