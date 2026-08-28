# Linux 原生 Wayland 窗口 resize 阻塞跟踪

## 状态

- 首次确认：2026-08-28
- 当前状态：`blocked`
- 影响验收：change `005-20260827-build-flex-and-space-layout-foundation` tasks 6.3、6.4、6.5；不得用 XWayland 结果标记原生 Wayland 通过
- 项目源码状态：所有诊断补丁均已撤销，没有把临时 libdecor/SDL 改动写入 RynUI

## 用户可见现象

- 在两个显示器上拖动窗口边框时，原生 Wayland 窗口都不能稳定实时跟随鼠标。
- 右侧 60 Hz、scale 1.0 输出表现为窗口尺寸带明显延迟，类似动画追赶鼠标。
- 左侧 240 Hz、scale 1.333333 输出更差，尺寸变化离散，快速拖动时几乎不跟随。
- `rynui_layout_demo`、Token Gallery 和最小 SDL/RynUI 窗口均能复现，说明问题不依赖 Flex/Space 页面复杂度。
- 同一个 Release 二进制强制使用 X11/XWayland 后，用户确认左右屏 resize 正常。

## 复现环境

```text
session=Wayland
desktop=GNOME 50.4
compositor=Mutter 50.4
system_libdecor=0.2.5-1
bundled_SDL=3.4.14
wayland_protocols=1.49-1
gpu_driver=Vulkan
output_left=eDP-1, 2560x1600, 240 Hz, scale 1.333333
output_right=HDMI-1, 1920x1080, 60 Hz, scale 1.0
```

基础复现命令：

```sh
SDL_VIDEO_DRIVER=wayland ./out/build/linux-gcc/examples/Release/rynui_layout_demo
```

诊断对照命令，不属于 task 6.3 验收：

```sh
SDL_VIDEO_DRIVER=x11 ./out/build/linux-gcc/examples/Release/rynui_layout_demo
```

## 已完成的排查

| 对照 | 结果 | 结论 |
|---|---|---|
| GCC Debug 原生 Wayland | 卡顿 | 不是 Release 优化差异 |
| GCC Release 原生 Wayland | 卡顿 | Debug validation 不是主因 |
| `rynui_minimal` 原生 Wayland | 同样卡顿 | 不是 Flex/Space、文字或复杂 scene 导致 |
| 限制 SDL 单次事件批量、合并 resize event | 无改善 | 不是 RynUI event backlog |
| Vulkan MAILBOX present mode | 无改善 | 不是单一 FIFO present-mode 问题 |
| libdecor 0.2.5 加上游 `RESIZING` bit，并让 SDL 读取 bit 8 | resize 可能停住，失焦才更新 | 单独移植状态位不足以修复当前 GNOME 50 路径 |
| 上述 libdecor 补丁，并让 SDL 每次 configure 都 commit | 恢复连续变化，但仍像延迟动画 | configure/commit 节流策略存在两难，不能作为正式修复 |
| 同一 Release 二进制使用 XWayland | 用户确认正常 | 问题锁定在 GNOME 原生 Wayland + libdecor/SDL configure 链，而不是 RynUI layout |

临时性能埋点还显示，Release 原生 Wayland 的单次 layout 最长约 3.1 ms，输入批量最多 5 个；持续拖动期间收到 353 个 resize event。事件存在且布局耗时不足以解释用户看到的长延迟。XWayland 同样可出现偶发较长 present 调用却保持视觉跟手，因此不能只用单次 present 最大值解释问题。

## 上游对应问题

SDL 官方 issue [Wayland: extreme lag when resizing window](https://github.com/libsdl-org/SDL/issues/13763) 与本机现象一致：最小 SDL3 程序在 Wayland resize 时严重延迟，切到 X11 后消失。SDL 维护者指出 GNOME 使用的 libdecor 没有把 resize flag 传给 SDL，需要等待新版 libdecor。

SDL 当前 Wayland 实现只在 libdecor 版本满足 0.3.0 时读取 `LIBDECOR_WINDOW_STATE_RESIZING`，并根据 resize 状态节流 configure commit，见 [SDL Wayland window source](https://github.com/libsdl-org/SDL/blob/main/src/video/wayland/SDL_waylandwindow.c)。libdecor 上游提交 [Give libdecor clients access to XDG_TOPLEVEL_STATE_RESIZING](https://gitlab.freedesktop.org/libdecor/libdecor/-/commit/8dc6b627ae1d5d4e286d01a6bed4c7b0e7af847d) 添加了该状态，但尚未包含在本机 0.2.5 release 中。

本机把该提交最小移植到 0.2.5 后仍未解决 GNOME 50 的完整交互，说明不能仅靠修改 SDL 版本判断或读取 bit 8 就关闭问题；需要上游正式 libdecor release 与 SDL/当前 GNOME 组合的重新验证。

## 临时运行方案

需要可用窗口 resize 时，可以显式运行：

```sh
SDL_VIDEO_DRIVER=x11 ./out/build/linux-gcc/examples/Release/rynui_layout_demo
```

该方案经过用户实机确认，但它运行在 XWayland，不是 OpenSpec task 6.3 要求的原生 Wayland，也不能替代跨输出 DPI、pointer 与字体 raster 的 Wayland 验收。

## 约束与后续动作

- 当前 change proposal 明确不新增第三方依赖，因此不能在 apply 阶段擅自打包 libdecor。
- task 6.3 明确禁止用强制 X11 代替 Wayland 验收，因此 Linux layout evidence 保持 `status=pending`。
- 若产品决定让 GNOME 默认回退 XWayland，需要先更新 OpenSpec 规划，明确 backend 选择、用户覆盖方式、跨输出 DPI/pointer 行为和真实窗口验收范围。
- 若坚持原生 Wayland，需要跟踪 libdecor 新 release，并在 GNOME/Mutter、本机混合刷新率与分数缩放输出上重新运行本文件的复现步骤。

## 关闭条件

只有同时满足以下条件才可关闭：

1. `SDL_VIDEO_DRIVER=wayland` 下窗口边框持续实时跟随鼠标，左右输出都无离散更新或动画式追赶。
2. 跨 scale 1.0 与 1.333333 输出后，UI logical size、字体清晰度和 Button pointer 命中保持正确。
3. `rynui_layout_demo` 完成 task 6.3 的全部交互、宽窄截图、诊断计数和正常退出。
4. Linux evidence contract 通过；XWayland 结果仍只作为独立诊断信息。
