# Image Linux 验收

> 本文件只记录源分支 `t3/native-image-upload`（最终 `217a5ea0`）的验证；不作为合并后 main 的通过证据。主线结果见 [Image/Upload 集成验收](image-upload-integration.md)。

- Linux x64 / GCC / Ninja Multi-Config，`linux-gcc-headless` Debug、Release 最终定向 CTest 各 **62/62**：18 Image component、14 Upload component、10 Upload model、7 Image resource、2 实际 Gallery，以及 Theme/backend/packing/effect、受影响目录/source/host/benchmark。公共 Image/Upload 头文件和文档示例 compile targets 在两个配置均构建通过；Core include/link guards 与实际 HEADLESS 构建通过。
- 最终命令（Release 将两个 preset 后缀 `debug` 换成 `release`）：

```text
cmake --build --preset linux-gcc-headless-debug --target rynui_media_gallery_tests rynui_gallery_sample_page_tests rynui_gallery_source_panel_tests rynui_ant_design_reference_catalog_tests rynui_gallery_pages_host_tests rynui_gallery_pages_benchmark rynui_upload_component_tests rynui_image_component_tests rynui_image_resource_tests rynui_portable_theme_algorithm rynui_portable_scene_backend rynui_portable_logical_scene_packing rynui_effect_range_benchmark -j2
cmake --build --preset linux-gcc-headless-debug --target rynui_image_public_api_compile rynui_upload_public_api_compile -j2
ctest --preset linux-gcc-headless-debug -R '^rynui\.(image_(component|resource)_|upload_(component|model)_|image_gallery$|upload_gallery$|portable\.(theme_algorithm|scene_backend|logical_scene_packing)$|effect_range_contract$|gallery_sample_page$|gallery_source_panel$|ant_design_reference_catalog$|gallery_pages_host$|gallery_pages_benchmark$|gallery_sample_source_)' --output-on-failure
```

- CPU 合同覆盖 PNG/JPEG/BMP 真实解码、畸形/超限/stride/alpha、事务失败重试、owner/epoch/退休、受控加载/错误/fallback、旧/重复/外线程 completion、取消重入/throw/self-destroy、zoom/rotate/flip/bounded drag、键控分组/最新 items/current/空列表恢复、RTL、保留工具栏及闲置零帧/零资源。stb_image 2.30 source/header hash、BUNDLED|SYSTEM 与 license 已集中锁定。
- 独立 Image packed ABI **2 / 112 bytes**；原 Quad/Glyph/RoundedEffect packed ABI v2 与 logical scene v3 不变。`linux-gcc` Debug / 私有 Mutter Wayland / Vulkan / SPIR-V 实际 GPU 读回 **49/49 checks**，含 RGB/半透明/全透明、非紧密源 stride、圆形裁剪、非法上传拒绝/失败重试/附件退休；不是 Recording 证据：

```text
cmake --build --preset linux-gcc-debug --target rynui_token_gallery rynui_image_gpu_tests -j2
python tools/run_background_wayland.py --log-dir out/media-gpu-final --timeout 45 -- out/build/linux-gcc/tests/Debug/rynui_image_gpu_tests out/media-gpu-final/readback
python tools/run_background_wayland.py --log-dir out/media-gallery-final --timeout 120 -- out/build/linux-gcc/examples/Debug/rynui_token_gallery --page-acceptance --page=image --sample=gallery.sample.image.native --page=upload --sample=gallery.sample.upload.native --page-output=out/media-gallery-final/result.json --page-timeout=110
```

- 最终原生页面 **2 cases / 247 checks / 50 submitted frames，passed**，系统字体、host input area/cursor sync。独立 Image 页面提供真实内存 PNG、可取消应用 loader 的 Loading/成功/失败、fallback、受控预览/分组、保留工具栏草稿；实际检查 RGB/alpha 读回、键盘缩放、bounded drag/capture/release、Escape、加载状态及整页退休。Upload 同时验证真实 Image 缩略图像素和应用合成传输；细节见 [Upload 结果](upload-linux.md)。
- Registry/source generator 实际 **89 samples**；相对 a0e7c7ed：**87 retained / 0 removed / 2 added**；原 **458** frozen legacy IDs 保留。五套 Theme golden 原 **61 个顶层字段**（除 identity）逐项不变，新增完整 Image/Upload 域，无字段剥除。父任务后续样例/Theme 增量由整合任务合并验证。
- `python tools/update_ant_design_gallery_catalog.py --check --self-test`、source generator 生成与 `--check`、`python tests/gallery_source_tests.py` 均通过；clang-format **22.1.3：961 files / 0 failures**；OpenSpec doctor healthy、strict **107/107**，diff/check clean。源码副本、JSON/BMP/log 均只在 ignored out。
- 先前扩大回归有两项旧测试失败：`portable.modal_boundary` 的 inside Tooltip、`portable.modal_component` 的 hidden Select idle frame；使用原 Modal 源且禁用新增 Image host 的对照仍为 **2/2 failures**。未计入本次通过数、未冒充修复，按用户通知不做无关重测；组合整合由父任务 63.* 跟踪。
- Windows/MSVC/D3D12、系统辅助技术、桌面/物理 IME 未运行。颜色/方向规则与支持格式见 renderer 合同；SVG/浏览器抓取不属于原生合同。
