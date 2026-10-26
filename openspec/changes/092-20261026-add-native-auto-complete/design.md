# Design

## Context

参见 proposal.md。已检查 Input 的真实 TextEditor/session、Select 的 retained rows/window-layer、DatePicker 的 configure_form_reset，以及 FormItem.control(InputRef)。锁定上游 AutoComplete.tsx 使用 combobox、无 suffix；doc FAQ 指明选项选择不触发 onSearch、空 options 不显示；tests/index.test.tsx 明确默认不过滤。参考 demo/basic、options、non-case-sensitive。

## Goals / Non-Goals

**Goals:** typed flat keyed options、真实 Input 和稳定 rich label、有限 popup 与 String Form 的可运行首版；保持 Core/renderer 边界与默认 1 MiB PE 栈。

**Non-Goals:** 不处理网络请求和竞态、不分组、不虚拟化、不动画、不提供 Web/RTL/任意编辑器替换或 popupRender；本轮没有 native GPU、Linux、系统 IME 或 AT 人工验收。rich label 的内容由 typed slot 构建并可自行绑定 reactive 数据；相同 key 的 slot identity 改变才替换该 label 子树。

## Decisions

1. 新 AutoCompleteModel 管理 String accepted value、query、active key 和临时 backfill。Input 使用受控 display Signal，所有真实编辑由 Input 执行；相同 value 不重复写入 editor。选择复制 option 与 callback 后再调用，回调之间重查完整 ComponentId，避免同步销毁后的访问。
2. 从 Select 已有有限列表机制按需实现独立 host，复用 WindowComponentParticipant、RetainedSurface 与公共 Popover 的祖先定位/ownership；不扩展 Select 的 selection 语义，也不抽象全库 popup。列表以 key 差分，保留 row/label；仅删除 key 或变更 rich slot identity 才局部卸载。
3. typed prefix/suffix 转换为 Input slots；选项 label 可有原生内容。主输入保持单 Tab 入口；装饰性 prefix/suffix 拒绝额外 focusable，popup 内丰富内容可拥有独立子 popup。
4. controlled open 只发意图，外部不接受时仍显示；输入边界可强制 suppress，恢复须新的显式打开。空过滤集合不渲染 popup。Tab/blur/Escape 清理 backfill；composition 时快捷键交还 Input，原始 commit 才产生搜索。
5. reset 复用 Input configure_form_reset，在 editor 重新读取前恢复字段 accepted String，清除 backfill/query/active/scroll；保持同值普通 prop 刷新不破坏 preedit。Form 的 disabled/readOnly 通过 Input 既有上下文生效，host 读取实际 editor/interaction eligibility 作为动作门禁。
6. AutoComplete Theme 使用 Select 风格 popup colors/metrics/typography/shadow/order 和 Input 选择框颜色；独立 override 全链追加，不改旧 IIFE 原文顺序。大量 ThemeConfig/ThemeSnapshot 临时值使用 heap，所有最终字体按 round 与 FontRuntime 上限校验，并保持 ThemeScope descendant preflight 原子性。
7. 仅本树 `windows-msvc-headless`、Ninja Multi-Config、MSVC Debug/Release，最多 2 jobs。依赖可引用 ROOT/out 的锁定 source 与字体缓存，所有生成物、cache、测试和 log 写入本树。五个 fresh Theme goldens 保留旧 typed leaves，额外检查 IIFE、ordinal、PE 与资源清理。测试集中 model、真实事件、Theme、clip/hit/focus、回调销毁、generation、owned block、idle/ref，不跑长 Gallery/native。

## Risks / Trade-offs

- [controlled value 拒绝意图] → 提交后展示与 accepted 权威一致，测试拒绝与延迟接受；composition 输入提交通过 onChange 驱动。
- [popup 后置布局导致 clip 陈旧] → 子 popup 定位先解析祖先；每次几何与 generation 更新重置 glyph/fill/hit clip。
- [回调销毁及 key 重用] → 每步用完整 generation id 重取；销毁取消 capture、移除 surface/fragment/interaction。
- [大 ThemeConfig 栈溢出] → 新构造使用 heap；验证 Debug/Release PE stack reserve 0x100000。
- [系统输入差异] → headless 合成 composition 仅证明门禁与 session 合同；系统 IME、AT 和原生 GPU 留给父后续验收，不计为已完成。
