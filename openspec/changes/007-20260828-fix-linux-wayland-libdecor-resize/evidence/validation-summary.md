# Wayland libdecor resize change 验收汇总

schema_version=1
change=007-20260828-fix-linux-wayland-libdecor-resize
status=passed
linux_status=passed
windows_status=passed
platform_generic_status=passed
archive_status=not-requested

## Linux BUNDLED 验收

- GCC 16.2.1：`linux-gcc` fresh configure、Debug/Release build 通过；Debug full CTest 138/138，Release 受影响测试 14/14。
- Clang 22.1.8：`linux-clang` fresh configure、Debug build 与受影响测试 14/14 通过。
- 原生 Wayland：GNOME/Mutter 50.4，240 Hz/scale 1.333 与 60 Hz/scale 1.0 双输出、双向跨输出 resize 均通过；`rynui_minimal`、layout 示例和 Token Gallery 均不再依赖 focus change 推进 configure。
- 实际运行时从 build-local staging prefix 加载 patched libdecor 0.2.5 core/cairo plugin；SDL GPU backend 为 Vulkan。详细环境、路径、计数和人工验收见 [Linux build evidence](linux-build-validation.md)、[state-machine smoke](linux-wayland-state-machine-smoke.md) 与 [native Wayland evidence](linux-native-wayland-validation.md)。

## Windows 依赖隔离与共享路径回归

- `windows-msvc` fresh configure、MSVC 19.51.36256.0 x64、Ninja Multi-Config Debug build 通过；full CTest 132/132。
- Windows configure 不创建 `RynUI::LibDecor` 或 `rynui_libdecor_external`；build graph、`_deps` 和 `dumpbin /dependents` 的 libdecor 命中均为 0。
- 共享 platform/font/glyph 路径额外执行真实 D3D12/DXIL Token Gallery smoke：window scale 1.5 与 acceptance render scale 1.0/1.5/2.0 共 4/4 正常退出。
- 首次 Windows fresh configure 发现 Linux patch tests 无条件要求 GNU `patch`；提交 `d70c07871c07a758c4007d80dacdec108d663cdf` 修正测试注册边界并加入真实 Windows graph/evidence contract。详细记录见 [Windows dependency isolation evidence](windows-dependency-isolation.md)。

## 主要提交

- `b891493`：锁定并构建 patched bundled libdecor。
- `3526eb1`：把 libdecor resize state 传给 bundled SDL。
- `fc39cf2`：按 frame callback pace configure/ack。
- `0e76e56`：完成 Linux patched dependency 构建与合同验收。
- `cff7913`：保留与输入同批到达的 exposed redraw request。
- `b60e889`：完成原生 Wayland 双输出 resize 验收。
- `d70c078`：完成 Windows/MSVC libdecor 隔离与共享 D3D12 回归。

## 保留边界与上游迁移

- 本修复只保证 Linux `BUNDLED` 模式的锁定 libdecor/SDL 组合；`SYSTEM` 完全由集成方提供，不自动下载、不应用补丁，也不继承本 change 的通过结论。
- 当前证据覆盖 build-tree RPATH 和 plugin discovery，不代表尚未定义的安装包布局已经完成；未来 packaging change 必须重新验证 installed RPATH、plugin path 与 license 分发。
- libdecor 或 SDL 升级时，patch context/hash 必须 fail-fast。只有上游 release 同时提供等价 resize-state、configuration lifetime 与 paced ack 行为，并完成原生 Wayland 双输出回归后，才可通过独立 dependency-upgrade change 删除本地 patch。
- Windows 从不消费 libdecor；后续 Linux patch 调整仍必须保留非 Linux target/build graph isolation contract。

## 最终校验

`openspec doctor --json`、`openspec validate --all --strict --no-interactive`、Windows full CTest、Linux full/affected CTest、dependency lock/license/patch contracts、真实窗口 evidence 与 `git diff --check` 均已有通过记录。所有 lock、license、patch、source、test 与 evidence 文件均由 Git 跟踪；本 change 实施可完成，但未收到 archive 请求。
