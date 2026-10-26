# Tasks

## 1. 规划与锁定基线（平台通用）

- [x] 1.1 核对 Ant Design 6.6.5 指定 HEAD、AutoComplete source/doc/demo/test 和 Input/Select/DateTime/Form 合同；交付简中 proposal/spec/design/tasks，通过 OpenSpec 1.14 doctor、strict validate 与 diff check 后本地规划提交。

## 2. 模型与主题（平台通用）

- [ ] 2.1 干净阶段合并父指定 6118f67728aede9985867377b122274ac86f31d7；实现 typed API/model 与完整追加 Theme 合同，交付对应 API/Theme 文档；以本树 windows-msvc-headless Debug/Release 模型与主题测试、五个 fresh goldens、旧 typed leaves/IIFE/ordinal 核对验证后本地提交。

## 3. 原生编辑与建议（平台通用）

- [ ] 3.1 实现真实 Input、keyed/rich label 保留、自由文本/选择/过滤/open authority/键盘/pointer/清空/backfill/IME/ref 与 Form reset；文档示例可编译，以本树 Debug/Release 实际事件和 Form 集成定向测试验证后本地提交。
- [ ] 3.2 完成有限 popup/scroll/子 popup 所有权及 Modal/Spin/hidden 边界，覆盖 glyph/fill/hit/focus clipping、回调销毁、generation、owned block 清理、idle/ref；以本树 Debug/Release 定向合同测试验证后本地提交。

## 4. 集成收口（平台通用）

- [ ] 4.1 运行适当 fresh Debug/Release 定向检查、五个 goldens、PE 1 MiB、format-code --check、OpenSpec doctor/strict 与 diff check；保存原始 log 和准确 evidence，记录测试数、实际 SHA 与未验 native GPU/Linux/系统 IME/AT，干净本地提交并交接父审查。

本轮平台通用证据使用 Windows MSVC、Ninja Multi-Config、windows-msvc-headless，最多 2 jobs。不重复无关全矩阵，不运行长 Gallery/native；本 change 不要求系统专属验收完成，不将其计为通过。所有 build/cache/log 在本树，依赖 source/font cache 只读引用父 ROOT/out。不 fetch/push/PR/archive，不创建 children。
