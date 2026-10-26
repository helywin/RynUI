# Proposal

## Why

RynUI 已有原生 Input 与 Select，但缺少允许任意 String 输入并辅助选择建议的 AutoComplete。新增真实保留的输入与建议流程，使用户能够组合 Form 而不把自由文本误建模为 Select selection。

## What Changes

- 新增 typed AutoCompleteProps、AutoCompleteOption、typed prefix/suffix/option-label slots、reactive value/options/open 与 InputRef。
- 支持 flat keyed 动态建议、String value/defaultValue、自由输入、显式过滤、键盘与 pointer 选择、清空、backfill、组合输入门禁及 Form reset。
- 保留 Input/editor 和相同 key 的 label 子树身份；复用 CPU scene v3、retained surfaces、window layer 与 popup ownership。
- 新增完整 AutoComplete Theme 派生、override、algorithm、inherit、序列化、hash、相等与增量 dirty 合同。旧 IIFE、TokenIdentity ordinal 与 typed golden leaves 保持原样，新项追加于 Avatar 后。
- 文档明确首版不含网络异步管理、分组选项、虚拟化、动画、RTL、Web、自定义原生编辑器与任意 popupRender；不公开这些未实现的 props。

## Capabilities

### New Capabilities

- `native-auto-complete`: 原生自由 String 输入、建议交互、主题、生命周期和 Form 组合。

### Modified Capabilities

无。

## Impact

新增公开 header、component/model、Theme 与定向测试；只在必要的 host/构建注册入口追加。基线从 83d26e7a 开始，在规划提交后的干净边界合并父指定 6118f67728aede9985867377b122274ac86f31d7。锁定只读参考为 Ant Design 6.6.5 HEAD 4a39f54842eade4e565ab336ef6097cd7e723cdd 的 AutoComplete 源码、doc、demo 与 test。其默认不过滤，选建议不触发 onSearch，空 options 不展示 popup。

风险集中在同步回调销毁、controlled authority、IME/backfill/reset 和后置 popup clipping。以本树 Windows MSVC headless Debug/Release 定向测试、五个 fresh golden、旧 IIFE/ordinal 核对和默认 PE 1 MiB 栈验证；不宣称 native GPU/Linux/系统 IME/AT 已验收。不修改 README、completion、Gallery、support-overlay 或 generated catalog；不操作远端、不创建子任务。
