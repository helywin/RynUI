# Splitter 与布局接口收口的通用验收

- 实际环境：Linux GCC，`linux-gcc-headless`，Ninja Multi-Config，BUNDLED 锁定依赖；所有构建 `-j 2`。
- 构建目标：`rynui_splitter_component_tests`、`rynui_shell_layout_component_tests`、`rynui_layout_splitter_gallery_tests`、目录/页面模型及受影响公共 Theme target。Gallery CPU 定义库实际编译。
- 相关组件/Gallery CTest：Debug `-R '^rynui\.(splitter_|shell_layout_)'` 27/27；Release 加目录/页面模型共 29/29。公共 Theme/目录 Debug 5/5，更新目录后 Release 2/2。
- 最终定向复验：Layout 的 Compact/control metrics、Header 居中、共享断点和页面，Debug 12/12（含 Theme golden），Release 11/11；Splitter 的重合分隔条、禁用取消和 Auto 图标状态修正后，Debug/Release 各 16/16。
- Splitter 覆盖：多面板有限约束求解、逻辑/百分比及过量/全零归一化、窗口变化、受控拒绝/回写、相邻尺寸守恒、真实 logical scene lazy 预览/释放提交/取消、重合分隔条方向选择、键盘/Home/End/RTL、双侧折叠恢复、全局/面板隐藏销毁策略、保留草稿、嵌套、异指针取消隔离、窗口失活/禁用/销毁释放、回调内销毁/抛错、自定义图标与双击、动画及 reduced motion、结束后停止请求帧。
- 两个独立 Gallery 页面实际挂载生产组件，测试输入分发、宽度变化时保留身份及销毁清理；源码摘录生成校验 74 个示例。目录生成、自测、clang-format 22.1.3（861 文件，0 失败）、Core/HEADLESS 依赖图与负向守卫、OpenSpec doctor、strict 全部校验 106/106、`git diff --check` 通过。
- 更新五份被 `theme_algorithm` 直接读取的确定性 Theme golden 输入；逐字段核对仅新增 layout/splitter 与对应 identity，既有字段不变。
- 结论：4.1/4.2、6.1/6.2 完成。未运行原生 GPU/窗口、系统 IME 或 Windows MSVC；7.1/7.2 和 Windows 由父线程统一收口。没有控制桌面或打开桌面测试窗。原始日志仅在 ignored `out/change106/`。
- 集成点：SiderBreakpoint 已 alias 到共享 Breakpoint，阈值调用 `Breakpoints::width()`，宽度来自 `services.responsive_layout().window_width()`。三个共享 helper 与 Grid 分支 `7724700c` 内容一致；main 已有时保留一份，在 WindowComponentServices 和 CMake 中各注册一次。合入 General 后合并 Theme 构造/比较/hash/TokenIdentity 字段，并按组合状态重新生成 Theme golden 与 Gallery 目录。未修改父线程原生 frame callback 等待逻辑。
- 交付仅本地 T3 提交；不推送 T3、不合入 main、不创建 PR 或 archive。
