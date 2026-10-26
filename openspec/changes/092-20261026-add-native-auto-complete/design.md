# Design

## Astra R1 修订

挂载先建立私有 Input/popup/list/affix region，再连接外部 Props，最后挂载富槽。外部连接使用单独持有、随原 owner 清理的 Scope，保证 connect_binding 首次同步读/apply 返回时 Scope 对象仍存在。每次用户代码或同步边界交付返回后，用完整 ComponentId/NodeId、直接父关系、主 Input interaction/editor 和所有私有 region ticket 重取状态；损坏结构只退役原 owner，不碰复用 index 的新代际。filter 在独立模型副本上执行，守卫每次 predicate 调用；富槽内 options/value/open/size 等变化由已安装的连接接收，行协调以 revision 与 pending 标记合并，避免忽略变化或重入半成品行。

首次 prepare 期间 Input count formatter 可能晚于 AutoComplete prepare 删除主 Input。因此 WindowComponentServices 在所有 prepare 完成后、LayoutEngine 遍历开始前调用一次 AutoComplete 私有结构复核；不在已经开始的节点遍历中补救损坏结构。LayoutStyle adapter 仅增加可选局部 guard，AutoComplete 用它包围初始 reader 与连接交付；现有调用仍使用默认空 guard。没有重构全局组件生命周期。

rich/native 后代和嵌套子 popup 的 Tab/ShiftTab 以主 Input 为全局遍历锚点，使用现有 FocusManager deferred focus 交付；prepare 观察整个逻辑后代的焦点退出。消费的上下/Enter/Escape 同样排队 keyboard modality，在同一次 keydown 事务结束时呈现真实焦点状态，回调后先核完整私有代际与编辑资格再触碰模型。

真实零尺寸只在节点已有 place_count、祖先完成定位后确定。此时抑制建议并持有原 owner 的输入 block，清理 focus/session/preedit；恢复正尺寸只解除自己的 block，保留 suppression，直到显式用户动作或新的 open authority。未布局的临时零值不抑制 defaultOpen/初始打开 Modal。清理复用唯一 Form reset hook。

Select 拆出逐 owner 的定位入口并加入局部递归 guard，复用 Popover 的既有祖先解析；解析同时识别 Select/AutoComplete/DateTime/Popover。先完成父行定位再测子 anchor，首帧、移动和滚动均在同帧使用最终几何，不交换全局 host 顺序。

refresh 另有局部交付代际：首次阻断富 Select 可同步触发关闭回调，并把父 open authority 改为 true。嵌套 refresh 已交付最新可见性后，旧 refresh 必须停止 label/style/block 的后续交付，避免原 owner 仍有效时覆盖较新的 Prop 更新。该路径以实际 row hit/输入资格复现，不把保留的 paint record 当作可交互证据。

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
5. reset 复用 Input configure_form_reset，在 editor 重新读取前恢复字段 accepted String，清除 backfill/query/active/scroll；保持同值普通 prop 刷新不破坏 preedit。Form 的 disabled/readOnly/status 通过字段适配属性显式连接，host 读取实际 editor/interaction eligibility 作为动作门禁。
6. AutoComplete Theme 使用 Select 风格 popup colors/metrics/typography/shadow/order 和 Input 选择框颜色；独立 override 全链追加，不改旧 IIFE 原文顺序。大量 ThemeConfig/ThemeSnapshot 临时值使用 heap，所有最终字体按 round 与 FontRuntime 上限校验，并保持 ThemeScope descendant preflight 原子性。
7. 仅本树 `windows-msvc-headless`、Ninja Multi-Config、MSVC Debug/Release，最多 2 jobs。依赖可引用 ROOT/out 的锁定 source 与字体缓存，所有生成物、cache、测试和 log 写入本树。五个 fresh Theme goldens 保留旧 typed leaves，额外检查 IIFE、ordinal、PE 与资源清理。测试集中 model、真实事件、Theme、clip/hit/focus、回调销毁、generation、owned block、idle/ref，不跑长 Gallery/native。

## Risks / Trade-offs

- [controlled value 拒绝意图] → 提交后展示与 accepted 权威一致，测试拒绝与延迟接受；composition 输入提交通过 onChange 驱动。
- [popup 后置布局导致 clip 陈旧] → 子 popup 定位先解析祖先；每次几何与 generation 更新重置 glyph/fill/hit clip。
- [回调销毁及 key 重用] → 每步用完整 generation id 重取；销毁取消 capture、移除 surface/fragment/interaction。
- [大 ThemeConfig 栈溢出] → 新构造使用 heap；验证 Debug/Release PE stack reserve 0x100000。
- [系统输入差异] → headless 合成 composition 仅证明门禁与 session 合同；系统 IME、AT 和原生 GPU 留给父后续验收，不计为已完成。
