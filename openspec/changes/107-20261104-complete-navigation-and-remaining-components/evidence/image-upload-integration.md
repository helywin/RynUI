# Image / Upload 主线集成验收

2026-11-05；main 基线 `d3431206d6c8a323aeecafffdb3124b4a62521cb`。按下列顺序语义合入，源工作树保持只读、干净、无 upstream，未推送 T3 分支；源分支结果 [Image](image-linux.md)、[Upload](upload-linux.md) 不计入本次主线通过数。

1. `dd21bb88f29b34ddc47c1cbe3b6dcae7a14cd88f`：共享 RGBA scene/resources/Vulkan。
2. `269f2cf94440a063f311a025a139368ccee0b4f8`：Image / PreviewGroup。
3. `644c47ac339e7a954d0bf2f3ed2073f110238e25`：Upload / file services。
4. `217a5ea043bb5547d5b4c53aebd8a6251a7d6512`：实际 Gallery / 文档。

保留主线 Form 的深路径、精确 Decimal、List、注册代际与异常清理；Upload 映射补齐真实 trigger geometry，同 UID 的 progress/status/error 继续发布。私有 trigger/富文件编辑器不占用外层 Item 的绑定或 Enter，显式嵌套 Item 继续自动绑定/提交。新增组合覆盖 reset 取消实际请求、disabled/readOnly、退休后的旧 ref/geometry 失效。初始文件列表在外围树仍挂载时更新 PreviewGroup，改用同宿主的活动构造事务及目标父级 Theme，避免后挂载入口重入。

Image 的 Escape 交给最内层 Popover/Modal 仲裁，键盘及滚轮只作用于顶层预览；动态 focus trap、嵌套/兄弟 Modal、恢复及销毁不解除仍需保留的输入边界。合入前两个指定 Modal 基线 **2/2 失败**已复现：隐藏 Select 定位重复失效已修复；Tooltip 旧测试误点 Tabs 的隐藏 overflow Button，改按 Tooltip owner 定位实际且有尺寸的 trigger，保留原开关/Escape 断言。两项及新增栈回归最终通过；未宣称完成 50.* 全部 Modal 功能。

Image 保持独立 packed ABI **2 / 112 bytes**，原 logical scene v3、Quad/Glyph/RoundedEffect packed ABI v2 不变。共同 preflight 补齐 retire 后的限制检查，实际拒绝 ABI1、缺 RGBA8/能力及超限输入，保留失败重试/epoch；仅死亡 slot 的空 scene 不分配 Image buffer，也不要求 Image capability。旧 binding lifecycle fixture 显式声明不支持 Image，保留原资源/设备析构顺序断言。Core include/link 守卫扩展至 src/image；stb_image 2.30 的 source/header SHA、license 与显式 BUNDLED|SYSTEM 继续锁定。现有 Mentions/Tree/TreeSelect/Transfer、focus/blur 异常清理和有序 Shift/Ctrl/Meta/最新 pointer metadata 未回退。

Linux x86_64 / GCC **16.2.1** / Ninja Multi-Config / BUNDLED；实际 HEADLESS 为 RECORDING。必要命令（Release 使用对应 `release` 后缀、相同 targets/正则）：

```sh
cmake --preset linux-gcc-headless
cmake --build --preset linux-gcc-headless-debug --target \
  rynui_gallery_document_model_tests rynui_media_gallery_tests rynui_gallery_sample_page_tests rynui_gallery_source_panel_tests rynui_ant_design_reference_catalog_tests \
  rynui_gallery_pages_host_tests rynui_gallery_pages_benchmark rynui_upload_model_tests rynui_upload_component_tests rynui_image_component_tests \
  rynui_image_resource_tests rynui_portable_theme_algorithm rynui_portable_scene_backend rynui_portable_logical_scene_packing rynui_effect_range_benchmark \
  rynui_image_public_api_compile rynui_upload_public_api_compile rynui_form_model_tests rynui_form_async_tests rynui_form_component_tests \
  rynui_form_advanced_tests rynui_form_interaction_tests rynui_form_lifecycle_tests rynui_form_integration_tests rynui_form_public_api_tests \
  rynui_form_theme_tests rynui_tree_form_integration_tests rynui_mentions_tests rynui_auto_complete_complete_tests rynui_theme_runtime_tests \
  rynui_theme_algorithm_tests rynui_token_gallery_frame_tests rynui_portable_modal_model rynui_portable_modal_public_api rynui_portable_modal_theme \
  rynui_portable_modal_boundary rynui_portable_modal_component rynui_portable_theme_public_api rynui_portable_theme_runtime rynui_portable_focus_state \
  rynui_portable_focus_lifecycle rynui_portable_frame_scheduler -j2
ctest --preset linux-gcc-headless-debug -j2 --output-on-failure -R '^rynui\.(image_(component|resource)_|upload_(component|model)_|image_gallery$|upload_gallery$|portable\.(theme_algorithm|scene_backend|logical_scene_packing)$|effect_range_contract$|gallery_document_model$|gallery_document_contract$|gallery_sample_page$|gallery_source_panel$|ant_design_reference_catalog$|gallery_pages_host$|gallery_pages_benchmark$|gallery_sample_source_)|^rynui\.(form_(model|async|component|advanced|interaction|lifecycle|integration|public_api|theme|documentation)|tree_form_integration|mentions_form|auto_complete_complete_form_editor|platform_input|text_input_events|theme_(runtime|algorithm)|dependency_lock|gallery_page_modal_frame|ant_design_gallery_catalog_(generator|contract)|portable\.(modal_(model|public_api|theme|boundary|component)|theme_(public_api|runtime)|focus_(state|lifecycle)|frame_scheduler|backend_boundaries))$'
```

平台通用集合 **Debug 96/96、Release 96/96，最终 0 未解决失败**：19 Image component、15 Upload component、10 Upload model、7 Image resource、2 实际媒体 Gallery，以及 Form/Modal/focus/frame/Theme/公开 API/依赖/目录/source/host/benchmark。两个公共 Image/Upload 文档示例 compile targets 均在 D/R 构建。Debug 首次 86 项执行通过，10 项因漏编 Upload model 测试 target 未运行；补构建后只补跑 **10/10**，不计未运行项为通过。最终资源修复影响的 Debug **9/9** 再验；Release 96 项已包含最终代码及新增 capability/空 scene 断言。未运行全量 CTest。

原生实际命令：

```sh
cmake --preset linux-gcc
cmake --build --preset linux-gcc-release --target rynui_token_gallery rynui_image_gpu_tests rynui_file_service_tests rynui_sdl_event_adapter_tests rynui_platform_input_tests rynui_platform_event_pump_tests rynui_platform_lifecycle_tests -j2
python tools/run_background_wayland.py --log-dir out/image-upload-integration/gpu-retirement-final --timeout 60 -- out/build/linux-gcc/tests/Release/rynui_image_gpu_tests out/image-upload-integration/gpu-retirement-final/readback
python tools/run_gallery_page_acceptance.py out/build/linux-gcc/examples/Release/rynui_token_gallery --log-dir out/image-upload-integration/native-final --preset linux-gcc --configuration Release --timeout 120 --page image --page upload --page form --page modal --page button
ctest --preset linux-gcc-release -R '^rynui\.(file_service_sdk|sdl_event_adapter|platform_input|platform_event_pump|platform_lifecycle)$' --output-on-failure
python tools/run_background_wayland.py --log-dir out/image-upload-integration/clipboard --timeout 30 -- out/build/linux-gcc/tests/Release/rynui_file_service_tests --clipboard
```

实际 **Vulkan/SPIR-V 像素读回 99 checks**：RGBA/半透明/全透明、非紧密 source stride、旋转/双向翻转、祖先 clip、圆形裁剪、非法上传拒绝/重试及附件退休。新增翻转读回首次在采样相位处失败，改为实际纹理像素中心后通过，保持原误差容限。此为实际 GPU，非 Recording。

最终私有 Mutter/Wayland/独立 D-Bus 的 **Image → Upload → Form → Modal → Button：5/5 cases、669 checks、140 submitted frames、140 compositor frame callbacks，0 失败**。system fonts、host input area/cursor sync 均 true；runner return code 0、desktop_fallback=false。覆盖真实 Image/alpha 缩略图读回、preview zoom/drag/capture/release/Escape、loading/error、Upload Ignore/progress/failure/retry/success/cancel-stale/remove/保留草稿、Form typed/exact decimal/deep List/异步/reset/Enter/滚动聚焦、输入/资源离页清理和后继控件；保留 change106 pacing 与正常 SDL pump。最后资源清理修复后已重跑此有界序列，不重复旧分类矩阵。

SDK/输入及旧 renderer binding 生命周期 **5/5**；其中 chooser SDK seam **26 checks**，私有 Wayland 同应用 `text/uri-list` fixture clipboard **7 checks**，分别统计。Drop.data 按锁定 SDL 临时所有权复制，不自行释放；外线程 chooser 只复制/排队，owner drain 和退休 ticket 控制交付。未调用真实 OS chooser/portal，未读取用户私有文件。默认宿主限 PNG/JPEG/BMP 图片 bytes：单张 16 MiB、聚合 64 MiB、4096 项，不递归跟随 symlink 目录；其他文件保留 metadata/locator。Gallery transport 明确是合成应用服务，无默认 HTTP 或服务器响应。

目录 write/check/self-test、source 生成/--check、source/API/token 检查通过：**93 samples = 原 91 全保留 + Image/Upload 2；458 frozen IDs 清单与基线字节一致**。五份完整 Theme golden 各 **64 顶层字段**，旧 **62** 字段除 identity 逐项不变。clang-format **22.1.3：991 文件、0 失败**；doctor healthy、strict **107/107**、diff/check 通过。原始日志/JSON/图片/二进制/源码副本只在 ignored out。

63.2 沿用主线已验收的 [Select/AutoComplete](select-autocomplete-integration.md)、[ColorPicker/InputNumber](color-number-integration.md)、[Cascader](cascader-integration.md)、[Date/TimePicker](picker-integration.md)、[Transfer](transfer-integration.md)、[Mentions](mentions-integration.md)、[Form](form-integration.md)、[TreeSelect](tree-treeselect-integration.md)、[其余已有录入控件](existing-data-entry-integration.md) 的实际后台分类证据，补齐本次 Upload 及组合序列后关闭。只新增完成 **25.1/25.2、34.1/34.2、63.2**，累计 **56/135**；63.3、64.*、65.1 保持开放。

真实 OS chooser/portal、跨应用 clipboard、实际 OS 拖入、Windows/MSVC/D3D12、物理系统 IME/AT、人工桌面与实际跨显示器/DPI 未验证。后台 GPU、SDK seam 与合成 composition 不代表这些平台体验。Release Gallery 供自行运行，最终交付重建后不启动到用户桌面。
