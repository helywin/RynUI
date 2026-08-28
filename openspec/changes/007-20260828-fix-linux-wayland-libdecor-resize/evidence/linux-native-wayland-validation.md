# Linux 原生 Wayland 双输出 resize 验收

- 日期：2026-08-28
- session：`XDG_SESSION_TYPE=wayland`，`WAYLAND_DISPLAY=wayland-0`
- SDL video driver：显式 `SDL_VIDEO_DRIVER=wayland`
- compositor：GNOME Shell 50.4 / Mutter 50.4
- kernel：Linux 7.1.8-arch1-3
- GPU/driver：SDL GPU backend `vulkan`；主机包含 Intel i915 与 NVIDIA GeForce RTX 5070 Ti Laptop GPU，NVIDIA driver 610.57.04
- preset/config：`linux-gcc` / `Release`
- bundled SDL：3.4.14，commit `147a8ee32dbf9ac02f3794964490687b6bbda1bc`
- bundled libdecor：0.2.5，上游 resize-state commit `8dc6b627ae1d5d4e286d01a6bed4c7b0e7af847d`
- 实际 libdecor core：`out/build/linux-gcc/_deps/rynui-libdecor-stage/lib/libdecor-0.so.0.200.5`
- 实际 libdecor plugin：`out/build/linux-gcc/_deps/rynui-libdecor-stage/lib/libdecor/plugins-1/libdecor-cairo.so`
- 未设置 X11 fallback；所有命令均显式使用原生 Wayland

## 输出配置

Mutter `DisplayConfig.GetCurrentState` 返回：

```text
left:  eDP-1, 2560x1600@240.001 Hz, scale=1.333333373
right: HDMI-1, 1920x1080@60.000 Hz, scale=1.0
layout: eDP-1 logical origin=(0,0), HDMI-1 logical origin=(1920,0)
```

## patch identity

```text
libdecor/0001 f4f1702b24ad3469ae934bc5e2233c21275d4882cf7374f6dd003f26001601f9
libdecor/0002 dffd9c9a4ec5542c9e3a99570613b56459094b18d11322ff31b3b2f388729bce
sdl3/0001     c3971c84d9056b53f0ccf388a0a22381963ebc9ab838a7fadfd85afeb559c04f
sdl3/0002     c76789442c18166ab3588a1d109caa6a9341766ca3144f77e3dca6c87f824741
```

## rynui_minimal

用户在左侧 240 Hz/1.333、右侧 60 Hz/1.0 分别快速连续 resize，并完成右→左跨输出后立即 resize。人工确认拖动持续跟随、松开后没有追赶队列，跨屏后窗口内容比例与鼠标位置正常；窗口正常关闭，退出码 0。

```text
display_scale=1.33333
window_size=1311x833
pixel_size=1748x1111
submits=1286
libdecor resize pacing: received=3490 replaced=3034 acked=456 frame=845
```

`3490 - 3034 = 456 = acked`。

## 公开 layout 示例与 redraw 回归

第一次验收公开 layout 示例时，用户确认拖动结束后仍需鼠标移出窗口才更新。失败诊断为：

```text
submits=97
libdecor resize pacing: received=1681 replaced=1673 acked=8 frame=93
```

根因是按需渲染示例把与 pointer input 同批到达的 `SDL_EVENT_WINDOW_EXPOSED` 一并过滤。提交 `cff7913` 为平台事件批次保留独立 redraw 标志，并让 layout/button/token consumer 即使同批存在输入也提交该 redraw；混合 expose+pointer 回归测试和完整 GCC Debug CTest 138/138 通过。

修复后用户重新快速 resize layout 示例，确认不再依赖鼠标移出且持续跟随；退出码 0：

```text
display_scale=1.33333
window_size=795x516
pixel_size=1060x688
viewport=795x516
submits=228
libdecor resize pacing: received=1068 replaced=927 acked=141 frame=205
```

`1068 - 927 = 141 = acked`。

## Token Gallery 双输出与跨输出

用户在两块输出分别快速连续 resize，并完成左右双向跨输出后立即 resize。人工确认 resize 持续跟手、松开后无旧尺寸追赶，Button 内外 hover 命中准确，跨屏后 UI 比例与字体清晰度正常；程序正常关闭，退出码 0。

最终位于 eDP-1 时，window host scale、render scale、drawable 与 logical viewport 使用同一当前输出指标；跨 scale 时 Gallery 重建 default font resolver，因此 glyph raster 使用当前 render scale：

```text
display_scale=1.33333
host_display_scale=1.33333
scale_source=window
pixel_density=1.33358
window_size=1364x850
pixel_size=1819x1133
viewport=1364.25x849.75
font_source=system
font_families=.PingFang_SC
font_rendering=aa=gray,hint=slight,rgba=unknown,lcd=default,bitmap=on
input_events=6122
submits=957
libdecor resize pacing: received=4266 replaced=3611 acked=655 frame=870
```

`4266 - 3611 = 655 = acked`。三个真实窗口均未通过 focus lost/gained 推进 resize，关闭时也没有 outstanding pending configuration。

## 结论

任务 5.2–5.4 在 GNOME/Mutter 原生 Wayland 的 240 Hz/1.333、60 Hz/1.0 和双向跨输出路径通过。XWayland 仅保留为历史诊断对照，没有用于本次验收。旧 change 005/006 中截图、主题外观和字体相位等更广泛平台清单仍按各自 tasks 独立完成，本证据只解除 resize 阻塞。
