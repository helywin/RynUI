## Why

RynUI 在 GNOME/Mutter 原生 Wayland 下通过 SDL3/libdecor 交互缩放窗口时，混合刷新率与分数缩放输出会出现明显延迟；当 SDL 识别到 resize 状态却不提交后续 libdecor configure 时，窗口甚至只在失去焦点后更新。最小示例同样复现且 XWayland 正常，说明需要在 Linux 平台依赖与 SDL Wayland configure/commit 边界修复，而不是继续调整 RynUI 布局或渲染循环。

## What Changes

- Linux `BUNDLED` 模式锁定 libdecor 0.2.5 源码、SHA256、MIT license 与上游 `XDG_TOPLEVEL_STATE_RESIZING` 四行补丁，在 RynUI build tree 内生成独立 libdecor core 与 decoration plugin，不修改系统安装。
- 在 SDL3 3.4.14 源码进入 CMake 配置前应用可审计补丁：对已确认提供 resize bit 的 bundled libdecor 去掉 `SDL_LIBDECOR_CHECK_VERSION(0, 3, 0)` 读取限制，不伪造 libdecor 版本号，也不解锁其他 0.3 API。
- 修复 libdecor interactive-resize configure 的确认与提交前进路径，使连续拖动期间的尺寸和帧持续更新，不再依赖窗口失焦或拖动结束触发一次性更新；不得以无限制同步提交制造新的 configure backlog。
- 增加补丁内容、应用顺序、依赖锁、license、target 隔离和错误路径合同测试，并在本机两个不同刷新率/缩放输出上完成原生 Wayland 真实窗口验收。
- `SYSTEM` 模式继续只使用集成方显式提供的 SDL3/libdecor，不静默回退到 bundled 补丁，也不把未验证的 system 组合报告为已修复。
- 非目标：不替换 SDL3 窗口/GPU/IME 后端，不修改公开 `ryn` API，不把 XWayland 作为原生 Wayland 验收结果，不在本 change 升级到 SDL development branch。

## Capabilities

### New Capabilities

- `linux-wayland-windowing`: 定义 Linux `BUNDLED` 平台依赖补丁、libdecor resize-state 传递、SDL configure/commit 前进保证及原生 Wayland 真实窗口验收合同。

### Modified Capabilities

无。

## Impact

- 影响集中依赖 lock、Linux `BUNDLED` resolver、第三方 patch 文件及其合同测试、SDL3 私有 Wayland 实现、Linux build-tree runtime/deployment 路径和真实窗口证据。
- Windows/MSVC、公开 C++ API、Reactive/Layout/Component、SDL GPU 与 IME 协议保持不变；Windows 不下载、不构建也不链接 libdecor。
- 主要风险是 libdecor 使用 Meson 与 runtime plugin，必须在 CMake/Ninja Multi-Config 外层保持确定的构建顺序、RPATH/plugin 发现和 Debug/Release 可重复性；configure/ack/commit 策略还必须避免把“失焦才更新”替换成持续但明显滞后的动画。
- 可验证结果是 `rynui_minimal` 与 layout/token 示例在 GNOME 原生 Wayland 的 240 Hz/1.333 与 60 Hz/1.0 输出上持续跟随交互缩放，拖动期间可见尺寸变化且失焦不再是更新条件，并且 build/test/dependency evidence 可证明实际加载的是锁定的 patched libdecor。
