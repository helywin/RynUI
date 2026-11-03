# 首批页面 frame 用例迁移

2026-11-03，实际 Linux / `linux-gcc` Release / Ninja Multi-Config / BUNDLED。

- 构建受影响 frame/sample/组件 targets：通过。定向 CTest：13/13 通过（3.30 秒）。包括 `gallery_page_input_frame`、`gallery_page_tree_select_frame`、`gallery_page_controller`、`gallery_sample_page`，以及 Button/Input/TextArea/OTP component 和 TreeSelect basic/controlled/keyboard/retained/boundaries。
- 原 `--input-only` 用例改用 Input 的活动家族工厂与登记的 test identity；移除其全目录最低组件数量和全局 input 数组偏移，保留输入/preedit/剪贴板/提交、Search Input/Button 所有权与键盘路径、TextArea variants/carets、OTP cells、Bidi、场景 identity/capacity、模拟 input area 和 idle 断言。
- 原 `--tree-select-only` 用例只挂载 TreeSelect 页面，按 sample/test ID 定位目标、备注、Form、反馈和命令，以显式当前页锚点代替全 catalog/Text 扫描。Default/Dark/Compact × 1200/560 六组合全部通过；保留真实业务接受/拒绝、rename/disabled/readonly/visible/reset、Form required/submit、popup wheel、键盘唤醒与滚动、glyph/fill/hit/focus 边缘及 retained graph 身份断言。
- 首批 Button 由页面夹具验证状态、六变体、ghost/block、延迟 loading/ref；其他旧页面、全局 inventory 假设和新目录 shell 的导航/几何仍分别属于 3.3–3.6、4.5，不从本次通过推断其完成。

以上为平台通用 Recording/CPU scene 与模拟文本平台验证，不包含真实 GPU、SDL 窗口或系统 IME。原始日志仅保留于 `out/105-implementation/20261103/`。
