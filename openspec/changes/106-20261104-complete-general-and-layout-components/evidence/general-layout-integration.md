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

组件任务 1.1/1.2、2.1/2.2、3.1/3.2、5.1/5.2 保持完成，共 **8/16**。Layout/Splitter 4/6 节保持待集成；7.1/7.2 总体任务等待全部组件与上述组合 GPU 问题收口；8.1/8.2 Windows 仍未勾选。不新增人工任务，不 archive 或创建 PR。
