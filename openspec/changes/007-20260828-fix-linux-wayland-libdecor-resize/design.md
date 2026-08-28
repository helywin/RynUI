## Context

见 `proposal.md` 的问题动机。现有 RynUI 锁定 SDL3 3.4.14，通过 SDL static target 使用系统检测到的 libdecor 0.2.5。`docs/issues/linux-wayland-resize.md` 已证明最小 SDL 窗口在 GNOME 50/Mutter 50 原生 Wayland 同样复现，RynUI 单次 layout 耗时和 event batch 不足以解释延迟，而同一二进制经 XWayland 正常。

libdecor 0.2.5 没有把 `XDG_TOPLEVEL_STATE_RESIZING` 写入公开 window-state bit。上游提交 `8dc6b627ae1d5d4e286d01a6bed4c7b0e7af847d` 只增加 enum bit 与 parser 分支，但尚未进入 release。SDL3 3.4.14 的 libdecor configure callback 又只在 `SDL_LIBDECOR_CHECK_VERSION(0, 3, 0)` 下读取该 bit。临时 A/B 已确认：同时补两处后，SDL 在 resize 首个 configure 后停止调用带 configuration 的 `libdecor_frame_commit()`，而 libdecor 会在 callback 返回时释放 configuration；后续 serial 无法延迟确认，因此可能直到失焦产生非-resize configure 才前进。简单改成每个 configure 同步 commit 虽能恢复连续变化，但会形成明显追赶动画，不能作为最终策略。

## Goals / Non-Goals

**Goals:**

- 在 Linux `BUNDLED` 模式提供来源、补丁、license 和实际加载路径均可证明的 patched libdecor 0.2.5。
- 只解除 SDL 对 `LIBDECOR_WINDOW_STATE_RESIZING` 读取的 0.3 version gate，不伪造版本，不开放 bounds、wm-capabilities 等其他 0.3 API。
- 为 libdecor configuration 建立可延迟到 frame callback 的明确所有权，按照“只保留最新 serial”的方式复用 SDL direct xdg-shell pacing，消除失焦才更新和逐 configure backlog。
- 保持 SDL3 Window/GPU/IME、RynUI logical coordinate 与按需帧架构不变。

**Non-Goals:**

- 不跟随 SDL 或 libdecor 的 moving branch，不把临时 patch 宣称为上游正式 0.3 release。
- 不要求 `SYSTEM` 模式修补发行版二进制，也不修改 `/usr` 下的库、pkg-config 或 plugin。
- 不用无限制同步 commit、busy redraw 或持续满帧掩盖 configure ownership 问题。
- 不在本 change 建立通用第三方 Meson package manager；集成只服务于 Linux libdecor 平台依赖。

## Decisions

### 1. 固定 libdecor 0.2.5 release，并按顺序应用两份最小 patch

集中 lock 新增 libdecor 0.2.5 archive URL、SHA256、MIT license、上游 resize-state commit identity 与 RynUI configuration-lifetime patch identity。第一份 patch 原样移植上游四行修改；第二份 patch 为 `libdecor_configuration` 增加内部引用计数及公开 retain/release 能力，使 client callback 返回后仍可安全持有 configuration，直至确认或替换。

不采用 libdecor master：当前 master 包含所需 bit，但版本仍声明为 0.2.2，且携带与本问题无关的未发布变化。也不把 0.2.5 版本改写为 0.3.0，因为那会让 SDL 编译进 patched library 未提供的 bounds、wm-capabilities 等接口。

configuration retain/release 只改变对象生命周期，不改变 serial、size、state 或 commit 语义。libdecor callback 自己持有一个引用；SDL 需要延迟时增加一个引用，替换或确认后释放。窗口销毁必须清空 pending 引用。

### 2. 由外层 CMake preset 编排 build-local Meson libdecor

Linux `BUNDLED` resolver 使用锁定 archive 建立 `ExternalProject`：Meson 禁用 demo、GTK 与 D-Bus，生成 libdecor core 和 cairo decoration plugin，安装到当前 build tree 的私有 staging prefix。cairo/pangocairo、Wayland client/protocol scanner 等作为显式 Linux 平台开发服务 fail-fast 检查，不进入公开 RynUI API。根构建仍由 `CMakePresets.json` 和 Ninja Multi-Config 驱动；child Meson/Ninja 只是该 external dependency 的受控构建步骤。

staging prefix 暴露一个内部 canonical target 与明确 build dependency。SDL 在 Linux `BUNDLED` 下直接链接该 target，并关闭动态探测任意系统 libdecor；示例和测试通过 build RPATH 找到 staged core，core 通过固定的 staged plugin directory 找到同批次 plugin。Windows preset 和 Linux `SYSTEM` resolver 不创建该 ExternalProject。

备选方案是重写 libdecor Meson 构建为项目自有 CMake target，或在 CMake configure 阶段同步执行 Meson。前者会长期维护第三方 source list/protocol generation，后者破坏 configure/build 分层和 Multi-Config 可预测性，因此均不采用。

### 3. SDL patch 在 FetchContent 配置前应用，并用 feature 而非假版本限定

SDL3 继续锁定官方 3.4.14 archive。`FetchContent_Declare(SDL3)` 增加严格、可重复应用的 patch step；补丁上下文不匹配时配置失败，防止 SDL 升级后静默漏补。

补丁只对 bundled build 做三件事：

1. 让 SDL CMake 接受外层提供的 build-local libdecor target/header，并记录 `SDL_LIBDECOR_HAS_RESIZING_STATE` 私有 feature define。
2. 在 `decoration_frame_configure()` 中以该 feature 读取 `LIBDECOR_WINDOW_STATE_RESIZING`，去掉这行读取对 `SDL_LIBDECOR_CHECK_VERSION(0, 3, 0)` 的依赖；其余 0.3 blocks 保持原 gate。
3. 通过 patched retain/release API 保存尚未确认的最新 configuration，并把确认时机接到 frame callback，而不是在每个 configure callback 内同步 commit。

`SYSTEM` 模式继续使用调用方的 `SDL3::SDL3`，不会对已安装 SDL 源码施加补丁。合同测试必须区分“patched bundled”与“integrator-owned system”，避免环境恰好装有 libdecor 时发生 system-first fallback。

### 4. resize 期间只保留最新 configuration，并在 frame callback 确认

libdecor configure callback 仍立即解析 window state、requested size、resize axis 并更新 SDL 的 pending metrics。当 `resizing=false` 时沿用即时 `ConfigureWindowGeometry()` 与 `libdecor_frame_commit(..., configuration)`。当 `resizing=true` 时：

- retain 当前 configuration，并原子地替换同一 UI 线程上的旧 pending configuration；旧引用立即 release，因此容量恒定为一个。
- 首次形成 pending 时发送 `SDL_EVENT_WINDOW_EXPOSED`，确保按需帧循环产生下一次 surface commit；已有 pending 时只更新最新值，不为过时尺寸重复排队。
- 对应 `wl_surface` frame callback 到达后，使用最新 requested metrics 计算 geometry，并以 retained configuration 调用一次 `libdecor_frame_commit()` 完成最新 serial 的 ack，然后 release。
- 若确认期间又收到 configure，下一帧继续同样过程；resize-clear configure 即时提交最终状态。窗口隐藏、销毁或 Wayland 断开时统一释放 pending 引用。

这与 SDL direct xdg-shell 的 `pending_config_ack`/frame callback 机制一致，但通过 libdecor configuration 引用解决原实现“callback 返回即失效”的限制。备选的“每个 configure 都同步 commit”已由本机 A/B 证明会恢复前进却保留追赶动画；“只提交第一个 resize configure”正是失焦才更新的来源，均不采用。

### 5. 以协议计数和双输出真实窗口共同验收

自动合同覆盖 patch hash、应用顺序、feature gate 范围、ExternalProject target/order、RPATH/plugin 路径、非 Linux隔离、SYSTEM 无回退以及 configuration retain/replace/ack/release 状态机。测试 seam 记录 received configure、replaced pending、acked serial、exposure request 和 outstanding reference，证明 steady state 只有一个 pending 且 serial 单调前进。

真实窗口只在 Linux 原生 Wayland 建立平台证据。使用本机 GNOME/Mutter，在 240 Hz/scale 1.333 与 60 Hz/scale 1.0 两个输出分别快速拖动 `rynui_minimal` 和公开示例边缘，并跨输出重复。证据记录 `XDG_SESSION_TYPE`、SDL video driver、compositor、输出 mode/scale、实际 loaded libdecor 路径与 patch identity、configure/ack/frame 计数、退出码和短录屏或连续截图。验收要求拖动期间更新，不以 focus change 触发最终 resize，也不在松开后继续明显追赶旧尺寸。

Linux GCC Debug/Release 完成 build 和受影响测试；Clang Debug 验证 ExternalProject/SDL patched header ABI 与 C++ target 集成。平台无关合同不在 Windows 重复，Windows 只运行依赖隔离 configure/build smoke，证明没有引入 libdecor。

## Risks / Trade-offs

- **[维护本地第三方 ABI 扩展]** → retain/release patch 保持小型、带独立合同与上游 commit 注释；升级 libdecor 时 patch context 必须 fail-fast，并优先迁移到上游等价 API。
- **[Meson external dependency 与 Ninja Multi-Config 顺序不一致]** → staged libdecor 使用单一稳定 C ABI 构建，所有消费 target 显式依赖 external target；Debug/Release 共用同一 staged platform library并分别链接验证。
- **[build RPATH 或 plugin path 泄漏到安装产物]** → build-tree 与 install/deployment path 分开测试；不存在受支持安装布局时不得把 build-tree 成功描述为 packaging 已完成。
- **[frame callback 不到达导致 pending serial 停滞]** → 首个 pending 必须请求 exposure；协议计数和最小窗口真实运行验证 exposed→present→callback→ack 闭环，禁止 busy loop 兜底。
- **[Mutter/libdecor plugin 仍有额外延迟]** → 双输出验收是完成条件；若 retain/coalesce 后仍出现明显追赶动画，任务保持未完成并以 Wayland trace 继续定位，不回退到“每 configure 同步 commit”。
- **[SYSTEM 模式行为不同]** → 明确由集成方提供且不宣称 patched；不允许 SYSTEM 失败后自动下载 BUNDLED。

## Migration Plan

1. 先提交 lock、license、patch fixture 与 resolver contract，使 patched dependency source 可复现但不改变非 Linux路径。
2. 接入 build-local libdecor core/plugin和 SDL feature patch，通过 Linux GCC/Clang build 与状态机测试。
3. 完成原生 Wayland 双输出验收后更新问题文档和相关 change evidence；只有对应实机证据通过才勾选平台任务。
4. 回滚时移除 Linux bundled libdecor resolver 和 SDL patch step，即恢复当前 SDL3 3.4.14 + integrator/system libdecor 行为；SYSTEM 与 Windows路径不需要迁移。
