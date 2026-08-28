# Linux Wayland resize 状态机 smoke

- 日期：2026-08-28
- 平台：Linux 原生 Wayland（未设置 `SDL_VIDEO_DRIVER=x11`）
- preset/config：`linux-gcc` / `Debug`
- 程序：`out/build/linux-gcc/examples/Debug/rynui_minimal`
- 启动环境：`SDL_VIDEO_WAYLAND_PREFER_LIBDECOR=1`，启用 SDL video debug logging
- 实际 libdecor core：`out/build/linux-gcc/_deps/rynui-libdecor-stage/lib/libdecor-0.so.0.200.5`
- 实际 libdecor plugin：`out/build/linux-gcc/_deps/rynui-libdecor-stage/lib/libdecor/plugins-1/libdecor-cairo.so`

## 交互结果

用户在窗口保持焦点时连续拖动调整大小，确认拖动期间窗口持续跟随，随后正常关闭窗口。未通过失焦或重新聚焦推动 resize。

退出前诊断如下：

```text
libdecor resize pacing: received=2255 replaced=1880 acked=375 frame=788
display_scale=1.33333
window_size=1230x640
pixel_size=1640x853
submits=1335
idle_wakes=584
idle_waits=3054
```

`received - replaced = 375 = acked`，说明所有未被合并的 retained configuration 均已确认；关闭时没有遗留 pending configuration。该结果验证任务 3 的最小窗口状态机 smoke，不替代任务 5 要求的 240 Hz/1.333、60 Hz/1.0 及跨输出完整验收。
