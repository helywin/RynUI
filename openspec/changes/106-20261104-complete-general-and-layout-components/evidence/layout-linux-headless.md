# Layout 平台通用验收

- 实际环境：Linux GCC，`linux-gcc-headless`，Ninja Multi-Config，BUNDLED 锁定依赖。
- Debug：构建 `rynui_shell_layout_component_tests`、`rynui_ant_design_reference_catalog_tests`、`rynui_gallery_page_model_tests`（包含 Gallery CPU 定义库）；`ctest --preset linux-gcc-headless-debug -R '^rynui\.(shell_layout_|ant_design_reference_catalog$|gallery_page_model$)'`，11/11 通过。
- Release：构建 `rynui_shell_layout_component_tests`；`ctest --preset linux-gcc-headless-release -R '^rynui\.shell_layout_'`，9/9 通过。
- 支持目录更新后重新构建并运行目录/页面模型，2/2 通过。目录生成校验、clang-format 22.1.3 全仓检查、OpenSpec doctor、strict 全部校验（106/106）及 `git diff --check` 通过。
- 覆盖：嵌套与双侧栏剩余空间、窗口宽度断点、受控请求/回写、零宽触发器与隐藏 autofocus、捕获/焦点/文本会话清理、保留草稿、主题通知与无效覆盖原子拒绝、自定义/隐藏触发器及回调内销毁。
- 结论：任务 4.1/4.2 的平台通用合同与 Gallery 页面接入完成。未运行原生 GPU/窗口或 Windows MSVC；对应任务保持未勾选。未控制桌面或打开桌面窗口。原始日志仅保存在 ignored `out/change106/`。
- 集成说明：公开正文入口命名为 `ContentArea`，避免与已有 `ryn::Content` 根 slot 类型冲突。后续 Splitter 阶段已按 Grid/Masonry 分支 `7724700c` 的接口统一共享断点：`SiderBreakpoint` 是 `Breakpoint` 别名，阈值调用 `Breakpoints::width()`，窗口宽度使用 `services.responsive_layout().window_width()`，Xs 阈值为 480 而非 Responsive 的默认覆盖规则。只复制该分支三个共享 helper 文件（`include/ryn/breakpoints.hpp`、`src/component/responsive_layout.hpp/.cpp`），未引入 Grid/Masonry 生产代码或提交。父线程集成时保留这些相同文件一份，并在 WindowComponentServices 的声明/构造/prepare 前发布及 CMake source 列表各保留一次。
