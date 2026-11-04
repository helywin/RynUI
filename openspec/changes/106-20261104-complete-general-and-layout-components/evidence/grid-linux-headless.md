# Grid 平台通用验收

- 实际环境：Linux GCC 16.2.1，`linux-gcc-headless`，Ninja Multi-Config，Debug / Release。
- 构建：`cmake --preset linux-gcc-headless`；分别使用 `linux-gcc-headless-debug` / `linux-gcc-headless-release` 构建 `rynui_grid_component_tests`、`rynui_grid_gallery_tests`、`rynui_ant_design_reference_catalog_tests`，`-j 2`。
- CTest：两个配置各 **13/13 通过，0 失败**。筛选 `^rynui.grid_|^rynui.ant_design_reference_catalog$|^rynui.ant_design_gallery_catalog_(generator|contract)$|^rynui.gallery_sample_source_(contract|generated)$|^rynui.portable.backend_boundaries$`。
- 其中 Grid 为 6 组组件合同和 1 组独立 Gallery 页挂载/缩放合同；覆盖窗口而非容器断点、逐字段覆盖、24 列/偏移/排序/位移、双轴 gutter、typed flex/约束、对齐/RTL、隐藏输入/焦点、编辑状态、身份及销毁。其余 6 项验证目录、源码生成及 Core/renderer 边界。
- `python scripts/format-code.py --check --clang-format /home/jiang/code/RynUI/out/linux-validation/format-tools/bin/clang-format`：clang-format 22.1.3，0 失败。`openspec doctor --json` 健康，strict 全部校验 **106/106**；`git diff --check` 通过。
- 结论：3.1/3.2 的平台通用合同完成。没有运行桌面窗口；本证据不代表 Linux GPU、系统输入服务或 Windows MSVC/原生验收。7/8 节仍由集成阶段追踪。
- 原始构建与测试日志仅保存在已忽略的 `out/validation/`。

补充回归：Stretch 现在先应用 Col 的外部 min/max height，再强制同代测量高度；显式高度仍保持。相关矩阵中的 `rynui.grid_alignment` 在 Debug / Release 各通过 1 项，覆盖六种 justify 下的 max-height 约束与 RTL。新增回归与局部修复单独本地提交，没有推送 T3 分支。
