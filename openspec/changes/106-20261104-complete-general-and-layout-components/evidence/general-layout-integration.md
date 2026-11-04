# General / Grid / Masonry 集成结果

2026-11-04，Linux x86_64 / GCC 16.2.1，`linux-gcc` Debug，Ninja Multi-Config。以 FloatButton 收口 `eebbc626` 为基线，顺序集成本地 Typography、Grid、stretch 修复、Masonry；未合入 Layout/Splitter。全部提交只保留在本地 T3 分支，由父线程集成 main，不 push T3 分支。

## 合并与平台通用证据

- 生成目录冲突以合并后的 source/support-overlay 重生成解决；保留所有 canonical 页、FloatButton Theme/宿主、共享窗口逻辑宽度断点、Masonry frame request revision、Typography 完整 UBA/真实字体分段/装饰及双 MIME 实现。
- 目录仍为 **73 entries / 7 categories**；implemented **15**、partial **38**、planned **19**、deprecated **1**。真实 sample/source 为 **70**；冻结 legacy inventory **458**，对应文件与基线逐字不变。新增断言防止四类组件支持状态或 FloatButton source 被合并覆盖。
- 复用已审读的 [FloatButton](float-button.md)（HEADLESS Debug/Release 各 17/17，后台 GPU 2 cases / 31 checks / 5 frames）、[Typography](typography.md)（HEADLESS 各 17/17、SDL mock 1/1）、[Grid](grid-linux-headless.md) 和 [Masonry](masonry-linux-headless.md)（合计相关 HEADLESS 各 22/22）阶段证据；没有重跑这些完整矩阵。
- 本轮 `cmake --preset linux-gcc`，构建 `rynui_token_gallery`、`rynui_text_clipboard_tests`、`rynui_grid_gallery_tests`、`rynui_masonry_gallery_tests`、`rynui_gallery_sample_page_tests`、`rynui_ant_design_reference_catalog_tests`、`rynui_gallery_page_acceptance_tests`，均使用 `cmake --build --preset linux-gcc-debug --target <targets> -j 2`，成功。锁定依赖源码只从父 `out` cache 读取，构建/产物留在本 worktree `out/`。
- 集中定向 CTest **11/11 通过**：Grid/Masonry canonical 页、Gallery sample/458 legacy、目录生成及合同、sample source 合同及生成、page options/runner、SDL clipboard mock。命令：

```text
ctest --preset linux-gcc-debug -R '^rynui[.](grid_gallery|masonry_gallery|gallery_sample_page|ant_design_reference_catalog|ant_design_gallery_catalog_(generator|contract)|gallery_sample_source_(contract|generated)|gallery_page_acceptance_(runner|options)|text_clipboard)$' --output-on-failure
```

公开 `rynui.hpp`、FloatButton/Typography/Grid/Masonry headers 的独立 C++20 `g++ -fsyntax-only -I include` 编译通过；native configure include/link 守卫通过，HEADLESS backend 边界沿用阶段证据。clang-format 22.1.3 检查 **863 files / 0 failures**；目录 `--check --self-test`、OpenSpec doctor、strict 全部文档 **106/106**、`git diff --check` 均通过。

## Linux 私有后台原生验收

新增可复用验收检查：Typography 键盘激活 HTML copy、完整纯文本/HTML MIME 自读与窄窗省略；Grid/Masonry 实际窗口 1000→500 逻辑宽度断点、输入草稿/身份保留；Masonry 高度变化/新增/重排/移除；离页销毁及编辑会话清理。使用真实 Vulkan / SPIR-V / system fonts，输入经组件宿主合成分发。所有窗口仅在 `tools/run_background_wayland.py` 私有 Mutter/Wayland；未控制用户桌面、系统鼠标键盘或桌面截图。

```text
python tools/run_gallery_page_acceptance.py out/build/linux-gcc/examples/Debug/rynui_token_gallery --preset linux-gcc --configuration Debug --timeout 60 --log-dir out/106-integration/native-masonry-isolated --page ant.component.masonry --page ant.component.button
```

单页 Masonry（Default 主题）→Button：**2 cases / 35 checks / 9 submitted frames，passed=true，return_code=0，desktop_fallback=false**。实际覆盖全部上述 Masonry 动态交互、断点、草稿及离页清理，不代表完整 GPU 像素矩阵。

```text
python tools/run_gallery_page_acceptance.py out/build/linux-gcc/examples/Debug/rynui_token_gallery --preset linux-gcc --configuration Debug --timeout 60 --log-dir out/106-integration/native-pages-recheck --page ant.component.typography --page ant.component.grid --page ant.component.masonry --page ant.component.button
```

组合序列为 Typography(Default)→Grid(Dark)→Masonry(Dark+Compact)→Button。**两次均超时，return_code=124、passed=false、desktop_fallback=false**；保留中途 **2 completed cases / 53 checks / 11 submitted frames**。日志确认 Typography 双 MIME 自读/缩放和 Grid 断点/草稿检查完成，随后 Masonry `insert` 动作的 GPU `submit_begin` 未返回；未到重排/移除/离页步骤。根因未定位，不能将单页成功归因为组合失败已修复，也不能将该序列报告为通过。首次验收代码误从 Button registry 查找 Typography text action 的定位错误已更正，不属于组件实现失败。

所有原始日志/JSON/二进制仅保存在 ignored `out/106-integration/` 与 `out/build/`，未提交附件。富文本原生检查仅为自身 MIME 回读，未验证跨应用消费端互操作；没有新增三类页面的 GPU 像素完整矩阵。Windows/MSVC、实际桌面、跨显示器、系统 IME 均未测。

## 任务边界

组件任务 1.1/1.2、2.1/2.2、3.1/3.2、5.1/5.2 保持完成，共 **8/16**。Layout/Splitter 4/6 节保持待集成；上述组合 GPU 阻塞已由下节定向修复，7.1/7.2 总体任务仍等待全部组件；8.1/8.2 Windows 仍未勾选。不新增人工任务，不 archive 或创建 PR。

## 组合阻塞定向修复

同日，以 main `ed4588625160a579de788909440566449764403a` 重建 `linux-gcc` Debug Gallery（最多 `-j 2`）后复现。定位结果：验收驱动把 GPU **submitted** 当作可立即推进下一次 resize/动态操作的边界，缺少 compositor frame 同步；在本机 NVIDIA 615.78.08 / 私有 Mutter 51.1 的 Wayland FIFO 路径触发交换链 image release 停滞。

- GDB 主线程栈为 `SdlSceneRenderer::submit_frame → SDL_WaitAndAcquireGPUSwapchainTexture → vkAcquireNextImageKHR → NVIDIA EGL → drmSyncobjTimelineWait`。对应 frame-in-flight fence 已释放，swapchain 为 1000×900 / 2 images，无待重建标记；不是该帧的 layout/upload/draw 等待。私有 compositor 主线程仍在 GLib poll；Wayland 双端记录显示 commit 已接收，后续 FIFO/frame 进度停止。
- 原组合在 `insert` 前保持 **2 cases / 53 checks / 11 frames**；Button→Button→Masonry→Button 同样将 Masonry 放在第三页 Dark+Compact，仍超时于 `reorder`，排除之前 Typography/Grid 状态为必要条件。原组合禁用全部 ordered scene draws、仅保留清屏的 GDB 对照仍停在同类 image acquire，排除实际组件 geometry/shader；单独加入 `SDL_SyncWindow` 和在阻塞处补一次 `SDL_PumpEvents` 均无效，没有保留这些试验。
- 修复仅作用于有限 Linux 原生验收：在布局/输入同步后、GPU surface commit 前登记 `wl_surface.frame`；提交后通过正常 SDL 事件等待取得回调，才推进下一个动作。回调等待最多 **5 秒**且受原 **60 秒**总期限限制，关闭/适配错误立即失败，RAII 清理 listener。保留全部 resize、主题顺序、键盘输入、动态 item 和离页检查。未改 renderer、Core、GPU ABI、资源 owner/epoch、调度器、驱动或 compositor 配置；普通交互运行不启用此同步。
- 同一修复后二进制的 GDB 消融仅强制 `done()` 返回 true、保留回调登记，原组合再次陷入同类 WSI 等待，证明实际等待边界有作用。反向故障注入强制返回 false，按预期在回调期限内以 exit **1**、`passed=false`、明确 deadline error 退出，记录 **1 submitted frame / 0 completed cases**；这不是成功验收 case。

新增两个仅 Linux 原生构建注册的可复用 CTest；均强制复用私有后台 Wayland runner，自动超时且不回退桌面：

```text
ctest --preset linux-gcc-debug -R '^rynui[.]gallery_(native_(general_layout|masonry_compact)|page_acceptance_(options|runner))$' --output-on-failure
ctest --preset linux-gcc-debug -R '^rynui[.]gallery_native_(general_layout|masonry_compact)$' --repeat until-fail:2 --output-on-failure
```

第一组 **4/4 通过**，包含 options/runner 2 项和实际 GPU 序列 2 项。原组合 **4 cases / 106 checks / 16 submitted frames / 16 frame callbacks**；Dark+Compact 对照 **4 cases / 72 checks / 11 submitted frames / 11 frame callbacks**。两者均为 **Vulkan / SPIR-V / system fonts、passed=true、return_code=0、desktop_fallback=false**，耗时约 **5 秒 / 3 秒**。第二组每条再连续运行两次，**4 次实际 GPU 调用全部通过**，覆盖全部 height/insert/reorder/remove、窗口断点、草稿与离页清理。

额外仅构建并运行受影响调度合同 `rynui.frame_scheduler`，**1/1 通过**；复用前述组件 HEADLESS 矩阵，没有重复完整 CTest 或原生矩阵。clang-format 22.1.3：**863 files / 0 failures**；目录生成/自检、OpenSpec doctor、strict 全部 **106/106**、`git diff --check` 通过。

原始 GDB/协议日志、故障注入、JSON、二进制仅存 ignored `out/106-gpu-fix/`，正式回归结果在 `out/gallery-pages/linux-gcc/Debug/`。frame callback 表示 compositor 帧边界，不等于物理 scanout 或完整像素等价；本次修复的是验收同步，不宣称驱动内部缺陷或通用 renderer 快速 resize 压力验证已完成。Layout/Splitter、Windows/MSVC、实际桌面、跨显示器和系统 IME 边界保持前述状态。
