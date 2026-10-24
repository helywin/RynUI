# 077 平台通用实施证据

## 环境与边界

2026-10-25，Windows x64，MSVC 19.51.36256.0，CMake `windows-msvc-headless` preset、Ninja Multi-Config，Debug/Release。独立工作树从 `0bc5c97b99520549576b2d9028c943f2450d69a2` 开始，构建目录为本树 `out/build/windows-msvc-headless`，依赖源码和构建目录均为本树 `out/deps`，未复用其他工作树二进制。所有 build 使用 `--parallel 4`。

## 阶段 1

实际运行：

```text
cmake --preset windows-msvc-headless -DFETCHCONTENT_BASE_DIR=<this-worktree>/out/deps
cmake --build --preset windows-msvc-headless-debug --target rynui_date_picker_model_tests rynui_date_picker_public_api_tests --parallel 4
ctest --preset windows-msvc-headless-debug -R "rynui.date_picker_(model|public_api)$" --output-on-failure
cmake --build --preset windows-msvc-headless-release --target rynui_date_picker_model_tests rynui_date_picker_public_api_tests --parallel 4
ctest --preset windows-msvc-headless-release -R "rynui.date_picker_(model|public_api)$" --output-on-failure
```

Debug/Release 各 2/2 通过。覆盖严格 ISO 往返/非法格式、1900/2000/2100 世纪闰年、年 1/9999 边界和空白格、42 格跨月日期、月份/年份及日导航、range clamp、禁用提交、清空、受控拒绝与回写。全禁用谓词不会驱动导航搜索；public API smoke 验证 reactive Props、ref 和 typed footer。

`python scripts/format-code.py --check` 使用 clang-format 22.1.3：588 个自有源文件通过；OpenSpec doctor healthy，strict validation 76/76，`git diff --check` 通过。

本证据为平台通用 headless 合同，不能证明 GPU、原生窗口/DPI、Linux 或系统 IME 行为。

## 阶段 2

DatePicker 已使用真实 retained Input、54 个固定标题/导航/日期节点（其中日期 42 个）、typed footer、窗口层、surface 与 glyph scene。实际读了锁定 upstream 的 date-picker/index.en-US.md、style/token.ts、style/panel.ts；该只读 checkout HEAD 核对为 `4a39f54842eade4e565ab336ef6097cd7e723cdd`。

```text
cmake --build --preset windows-msvc-headless-debug --target rynui_date_picker_model_tests rynui_date_picker_public_api_tests rynui_date_picker_component_tests rynui_date_picker_theme_tests rynui_portable_theme_algorithm --parallel 4
cmake --build --preset windows-msvc-headless-release --target rynui_date_picker_model_tests rynui_date_picker_public_api_tests rynui_date_picker_component_tests rynui_date_picker_theme_tests rynui_portable_theme_algorithm --parallel 4
ctest --preset windows-msvc-headless-debug -R "rynui.date_picker_|rynui.portable.theme_algorithm$" --output-on-failure
ctest --preset windows-msvc-headless-release -R "rynui.date_picker_|rynui.portable.theme_algorithm$" --output-on-failure
```

Debug/Release 各 5/5 通过。component 测试运行真实窗口组件服务、测试字体 shaping、logical glyph/surface/hit store 和 Input 编辑器，覆盖：

- 指针打开/闰日选择/清空、月份及年/周/日键盘移动、Home/End、Enter/Escape/Tab 与焦点恢复；三个 size、四个 variant、status 转发。
- 无效及禁用草稿不发布、blur 恢复、受控值拒绝/回写、未完成 IME 的 Enter/导航/Escape 门禁。
- 范围响应更新不改写选择，readOnly 仍可月历选择，全禁用规则只有有界资格判断；禁用时不通过 blur 提交草稿。
- 受控 open 拒绝、一次外部点击的 blur/pointer 关闭请求去重、禁用/窗口失活/隐藏祖先后不自发重开，包括未聚焦的 defaultOpen 弹层。
- Modal 内 Spin、较高 Modal、拥有子 popup 的 footer Select、最近实际 hit layer 的滚轮归属，以及 TextArea 内容滚轮先于日历边界 fallback。
- Input 与 footer 节点/编辑器/草稿/会话跨月、status、Dark/Compact/Default 更新保留；footer IME Escape 优先；纯颜色不重新测量并到达实际 glyph material。
- 窄/短/零可用高度后的实际 glyph、surface 与命中范围、从零高度恢复身份、visible idle 不重复测量/表面更新且无持续帧请求；四类回调销毁及非法 Props 清理。

Theme 测试覆盖默认/Dark/Compact、继承/reset、局部 algorithm 及未启用 seed、snapshot identity/equality/diagnostic、颜色/度量/effects/order 的 dirty 分离、非法字体/度量的原子拒绝。新增解析继续使用 reference IIFE，没有改写原有 26 个 IIFE、Prop heap bridge 或 commit_snapshot ref guard。

用本树新构建的 `tests/portable/Debug/rynui_portable_theme_algorithm.exe --write-goldens` 重新生成 default、dark、compact、dark-compact、compact-dark 五份 golden。逐份 JSON 与阶段 1 提交 `e515841` 比较：移除新 `datePicker` 和 `identity` 后全部旧字段深度相等；五份 identity 均实际变化，golden 测试两配置通过。未手改旧字段。

API 示例随 public API target 实际编译。格式检查 595 个自有源文件通过；doctor healthy、strict 76/76、`git diff --check` 通过。测试是 headless 注入的 composition 事件，没有宣称系统 IME、GPU、Linux 或 Gallery 验收。
