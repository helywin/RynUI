# Linux 固定字体的整数排版位置

固定的 Linux Fontations 光栅路径显式启用整数排版位置。HarfBuzz 保留源字面精度完成排版，再逐字形对 advance 和 offset 取整。取整使用 logical 像素，DPI 不改变该排版结果。其他光栅路径维持各自选择的排版精度。[Chromium HarfBuzz 排版例程](https://chromium.googlesource.com/chromium/src/%2B/c09887405b8deb565f8ad16b37ae35467188ef4f/third_party/blink/renderer/platform/fonts/shaping/harf_buzz_shaper.cc)

按 `linux-gcc-release` 的编译与链接规则重建字体模块，`rynui_font_runtime_tests` 通过，覆盖原有分数位置和四档 DPI 下的整数位置。真实私有 Wayland/Vulkan 以 1× DPR 重放受控 Input 的三个检查点。样例标题末字的 3 logical px 累积偏移消失，提交与拒绝画面的光标横坐标从 89 移到 Chromium 实际位置 91。该证据覆盖字体、布局位置和读回，三例联合 profile 与 meta 仍需独立验收。
