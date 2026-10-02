# 平台通用证据

## 阶段 1

2026-10-02，Windows MSVC 14.51，Ninja Multi-Config，`windows-msvc-headless`；使用 VS Developer Environment 与 UTF-8 console。

```text
cmake --build --preset windows-msvc-headless-debug --target rynui_portable_radio_component rynui_portable_radio_features rynui_portable_radio_public_api rynui_portable_selection_component rynui_portable_selection_controls_public_api rynui_portable_focus_order rynui_portable_focus_state rynui_portable_focus_lifecycle
ctest --preset windows-msvc-headless-debug -R "rynui.portable.(radio_component|radio_features|radio_public_api|selection_component|selection_controls_public_api|focus_order|focus_state|focus_lifecycle)$" --output-on-failure
```

Release 使用同名 release preset。Debug 8/8（1.59s），Release 8/8（1.08s）；类型值、受控等待回写、最近组、回调复制/销毁、动态重排/删除/capture/空集恢复、ref 线程/禁用/解绑复用与旧用法通过。Focus 合同确认非 Tab 项仍允许程序焦点。

clang-format 22.1.3：407 自有源，0 failures；OpenSpec doctor healthy，严格校验 48/48，diff check 通过。按钮/独立 Radio token/反馈属于阶段 2；此证据不代替真实窗口或其他平台证据。
