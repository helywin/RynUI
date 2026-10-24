# Tasks

## 1. 平台通用：Form 样例

- [x] 1.1 在已合入的 075 上追加真实 Form 和四种已有字段类型、提交/失败聚焦/重置/禁用/只读操作及稳定标识；Form 目录直达样例，增加聚焦 Recording 入口并纳入完整 frame，验证真实输入和错误/结果、同值数字草稿 reset、身份及三个 Theme/两种宽度 clip；实测 inventory，更新 Form API docs/支持范围，通过 generator/八项 contracts、格式、doctor、strict 与 diff check。

验收证据：[Form 阶段 MSVC Recording](evidence/stage1-form-msvc-recording.md)。平台通用验证实际使用 Windows / `windows-msvc-debug`；整体进度仅 1/4。

## 2. 平台通用：DatePicker 样例

- [ ] 2.1 077 独立验收并合入后追加真实月历选择/ISO 输入/清空与限制样例，提供显式 Form String 日期适配；目录直达，聚焦 Recording 检查实际目录/键盘/指针选择、提交和重置、草稿/身份/有限 glyph/hit clip，覆盖三个 Theme/两种宽度；更新文档/partial 范围和实际 inventory，通过 generator/contracts、格式、doctor、strict 与 diff check，不引入组件未承诺能力。

## 3. 平台通用：TimePicker 样例

- [ ] 3.1 078 独立验收并合入后追加真实时间列/输入/清空、确认/取消与限制样例，提供显式 Form String 时间适配；目录直达，聚焦 Recording 检查真实目录/列滚动/键盘/指针、提交和重置、草稿/身份/有限 glyph/hit clip，覆盖三个 Theme/两种宽度；更新文档/partial 范围与实际 inventory，通过 generator/contracts、格式、doctor、strict 与 diff check。

## 4. 集成验收

- [ ] 4.1 全部样例齐备后，在 Windows MSVC windows-msvc / Ninja Multi-Config 实际构建 Gallery，统一通过完整 token_gallery_frame 与八项 contracts 及原生 --smoke；由 Astra xhigh 复核完整阶段与已知问题修复，记录准确命令、实际 inventory 和平台边界，更新完成清单。Recording/启动 smoke 不替代组件人工视觉、系统 IME、Linux 或辅助技术验收。
