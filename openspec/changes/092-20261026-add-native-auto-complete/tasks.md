# Tasks

## 1. 规划与锁定基线（平台通用）

- [x] 1.1 核对 Ant Design 6.6.5 指定 HEAD、AutoComplete source/doc/demo/test 和 Input/Select/DateTime/Form 合同；交付简中 proposal/spec/design/tasks，通过 OpenSpec 1.14 doctor、strict validate 与 diff check 后本地规划提交。

## 2. 模型与主题（平台通用）

- [x] 2.1 干净阶段合并父指定 6118f67728aede9985867377b122274ac86f31d7；实现 typed API/model 与完整追加 Theme 合同，交付对应 API/Theme 文档；以本树 windows-msvc-headless Debug/Release 模型与主题测试、五个 fresh goldens、旧 typed leaves/IIFE/ordinal 核对验证后本地提交。

## 3. 原生编辑与建议（平台通用）

- [x] 3.1 实现真实 Input、keyed/rich label 保留、自由文本/选择/过滤/open authority/键盘/pointer/清空/backfill/IME/ref 与 Form reset；文档示例可编译，以本树 Debug/Release 实际事件和 Form 集成定向测试验证后本地提交。
- [x] 3.2 完成有限 popup/scroll/子 popup 所有权及 Modal/Spin/hidden 边界，覆盖 glyph/fill/hit/focus clipping、回调销毁、generation、owned block 清理、idle/ref；以本树 Debug/Release 定向合同测试验证后本地提交。

## 4. 集成收口（平台通用）

- [x] 4.1 运行适当 fresh Debug/Release 定向检查、五个 goldens、PE 1 MiB、format-code --check、OpenSpec doctor/strict 与 diff check；保存原始 log 和准确 evidence，记录测试数、实际 SHA 与未验 native GPU/Linux/系统 IME/AT，干净本地提交并交接父审查。

本轮平台通用证据使用 Windows MSVC、Ninja Multi-Config、windows-msvc-headless，最多 2 jobs。不重复无关全矩阵，不运行长 Gallery/native；本 change 不要求系统专属验收完成，不将其计为通过。所有 build/cache/log 在本树，依赖 source/font cache 只读引用父 ROOT/out。不 fetch/push/PR/archive，不创建 children。

## 5. Astra R1 修复（平台通用）

- [x] 5.1 在 clean bca1acafde1c7486d549bc9104a3e2f836b937ba 的实现上构建五个独立实际事件复现用例，记录首次挂载访问违规、rich Tab、真实零尺寸、root AutoComplete 子 Select 定位、keydown modality 五项失败的原始日志。
- [x] 5.2 修复同步挂载/连接/富 region/filter/主题/首次边界的完整 owner 与私有代际守卫、富槽内 Prop 更新，避免持有释放后的 Scope/State；覆盖 owner/private child 退役、回调重绑和 formatter。
- [x] 5.3 修复主 Input 遍历锚点与整个逻辑后代退出、真实布局零尺寸输入清理及恢复、祖先依赖定位和消费键盘命令的 keyboard 焦点交付；覆盖实际 glyph/fill/hit/focus 与 callback 销毁安全。
- [x] 5.4 本树 fresh MSVC HEADLESS Debug/Release 运行原有 19 项、新增 5 项及直接受影响的 DateTime/Popover/Input/Form/Select 代表集合；保持旧 Theme IIFE/ordinal/typed goldens、唯一 resetHook 与默认 PE 1 MiB，运行 format、显式 OpenSpec 1.14 doctor/strict、diff，提交准确 before/after evidence，clean HEAD 交父 fresh R2。Theme 产物未变时不重生成金样，不运行无关全矩阵。
- [x] 5.5 补充首次子 Select 关闭回调同步回显父 open 的实际复现；局部 refresh 代际停止旧一轮 label/style/block 交付，Debug/Release 重验 37 项集合后独立提交。

## 6. Astra R2 修复（平台通用）

- [x] 6.1 在 clean 0c3ab536f4500a057ce81f84f7cd4d77887ea758 上先实际复现局部 catch 后首次布局越界及 rich 后代同帧两次 focus 后建议残留，保存 before 日志。
- [x] 6.2 首次 owner/slots/bindings/reconcile 完整成功交接前发生异常时退役原完整 generation 并重抛；后续 reactive 新增/替换 rich label 同样保留原异常且不留下半成品，不破坏回调建立的新 generation。验证 fallback、后续 Signal/ref/session/block/subscription 清理。
- [x] 6.3 观察实际完整逻辑后代的焦点转移，覆盖同帧 Input/外 Button 两个出口、嵌套 popup、后代内部转移、controlled open 拒绝及关闭回调销毁/重绑；保留后代原生 onFocus/onBlur 与身份。
- [ ] 6.4 最终源码仅运行各配置一次 fresh MSVC HEADLESS Debug/Release 新回归、R1 和直接共享代表集合，最多 2 jobs；不重跑金样/无关全矩阵。完成 format、显式 OpenSpec 1.14 doctor/strict、diff、默认 PE 1 MiB 与原 golden 不变检查，英文 local commit、clean HEAD 交父 fresh R3。
