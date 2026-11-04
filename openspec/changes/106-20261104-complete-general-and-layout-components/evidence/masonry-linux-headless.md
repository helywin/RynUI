# Masonry 平台通用验收

- 实际环境：Linux GCC 16.2.1，`linux-gcc-headless`，Ninja Multi-Config，Debug / Release。
- 构建：两个配置分别构建 `rynui_masonry_component_tests`、`rynui_masonry_gallery_tests`、`rynui_grid_component_tests`、`rynui_grid_gallery_tests`、`rynui_ant_design_reference_catalog_tests`，均 `-j 2`。
- CTest：使用 `linux-gcc-headless-debug` / `linux-gcc-headless-release`，筛选 `^rynui.(masonry|grid)_|^rynui.ant_design_reference_catalog$|^rynui.ant_design_gallery_catalog_(generator|contract)$|^rynui.gallery_sample_source_(contract|generated)$|^rynui.portable.backend_boundaries$`，两个配置各 **22/22 通过，0 失败**。
- 其中 Masonry 8 组组件合同 + 1 组独立 Gallery 页，Grid 回归 7 项，共享目录/源码/边界 6 项。覆盖真实高度最短列、指定列/高度、窗口断点与 RTL、typed 数据更新、keyed 增删/重排、保留输入草稿、动态尺寸、仅分配变化回调、回调修改列数/销毁、无效/极端值、失败回滚与显式后续重试、fresh 空闲帧及销毁清理。
- Gallery 页还通过合成指针点击改变高度、添加、重排后移除项目，检查真实测量及保留身份。全部为 HEADLESS CPU/输入合同，不是桌面或 GPU 结果。
- 公共 Grid/Masonry header 的独立 C++20 `-fsyntax-only` 编译通过。clang-format 22.1.3 全部自有源码检查 **0 失败**；`openspec doctor --json` 健康，strict 全部校验 **106/106**，`git diff --check` 通过。
- 结论：5.1/5.2 的平台通用合同完成。Windows MSVC、Linux/Windows 原生窗口、GPU/驱动/系统输入服务仍未在本阶段验证；没有勾选 7/8 节任务，没有启动桌面窗口。
- 原始构建、测试和缓存诊断日志仅存放在已忽略的 `out/validation/`。本阶段遵照最新指令仅本地提交，不推送 T3 分支。
