# Linux 原生后台集成

2026-11-03，实际 Linux / `linux-gcc` Release / Ninja Multi-Config。相关最终 Gallery 构建通过。

- 命令：`python tools/run_gallery_page_acceptance.py out/build/linux-gcc/examples/Release/rynui_token_gallery --preset linux-gcc --timeout 20 --log-dir out/105-implementation/20261103/linux-native-pages`。runner 内部固定调用 `tools/run_background_wayland.py`，使用独立 Mutter headless virtual monitor、dbus-run-session 与私有 XDG_RUNTIME_DIR；没有回退桌面。
- 原生入口执行 1 次，结果 **timeout**，退出码 124。已记录 cases=2、checks=27、submitted_frames=5。
- 可取得的实际元数据：preset=linux-gcc，configuration=Release，toolchain=GNU，backend=vulkan，shader=SPIR-V，font_source=system。
- 过程记录：总览与 Input 完成，共 5 帧、27 个已记录检查、2 个完成 case；Select 页已完成 layout/upload，最后停在一次 renderer submit_begin，没有 submit_complete。仅凭该超时不能确定驱动或组件根因。system_fonts、host_input_area、host_cursor_sync 已记录为 true，完整矩阵仍未通过。
- 入口结论：Private native test timed out; no desktop fallback was started。

该结果不替代用户桌面、跨实际显示器、系统 IME 候选窗或 Windows/MSVC 验收。参数拒绝、超时与证据 schema 的 CPU 检查不推导 native 通过；未通过项保持未完成。原始 native/compositor log 和 JSON 仅在 out。
