# Breadcrumb 实现验收

平台：Windows / MSVC 19.51 / Ninja Multi-Config / windows-msvc-headless，HEADLESS/RECORDING，每次 build `--parallel 4`。

```text
cmake --build --preset windows-msvc-headless-debug --target rynui_breadcrumb_component_tests rynui_breadcrumb_theme_tests rynui_navigation_model_tests rynui_portable_theme_algorithm --parallel 4
ctest --preset windows-msvc-headless-debug -R '^rynui.(breadcrumb_|navigation_model|portable.theme_algorithm)' --output-on-failure
cmake --build --preset windows-msvc-headless-release --target rynui_breadcrumb_component_tests rynui_breadcrumb_theme_tests rynui_navigation_model_tests rynui_portable_theme_algorithm --parallel 4
ctest --preset windows-msvc-headless-release -R '^rynui.(breadcrumb_|navigation_model|portable.theme_algorithm)' --output-on-failure
```

Debug/Release 各 4/4。覆盖实际祖先 pointer/key 激活及 key/index、current 不激活、repeat/keyup、禁用→启用真实 HitTest 恢复、隐藏末项/中项 separator 投影、Flex gap 全隐藏重排/恢复身份、retained 富 slots、子 Button/Input 输入所有权、隐藏 Input blur 回调销毁、回调释放全部交互和 retained effects。有限宽 `280/40/4/0` 与高 `48/12/4/0` 检查活动分支的实际 glyph、surface 与 HitTest clip；隐藏缓存 glyph 不计为实际绘制。

独立 Theme colors/metrics/effects、父覆盖及 inactive seed、子覆盖分层、局部算法、最终派生字体/非有限度量/负数的原子拒绝通过。材质变化不触发 measure；空闲无待请求帧或 deadline。

`rynui_portable_theme_algorithm --write-goldens` 后执行 `compare-baseline.py`：5 份 fresh golden 的 14,032 个旧叶字段类型和值保持，原 28 个 IIFE 原文与顺序保持。允许变化仅 identity 与新 breadcrumb 域。clang-format 22.1.3、doctor、strict 与 diff check 在本阶段提交前通过。

此证据不包含 Gallery、原生窗口/GPU、Linux、系统 IME 或辅助技术验收。
