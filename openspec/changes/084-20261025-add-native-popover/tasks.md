# Tasks

## 1. 规划

- [x] 1.1 核对本地锁定参考与现有 popup/Theme 合同，完成 proposal/spec/design/tasks，经 doctor、strict validate 与 diff check 后本地提交。

## 2. 平台通用实现与验收

- [x] 2.1 新增 typed API、开闭/触发/延时 model 与独立 Theme 完整链，验证 public API/model/Theme 测试；保留旧33个 IIFE、append-only identity，生成五份 goldens 并深比较旧字段。
- [x] 2.2 实现 retained Popover host 与真实 Input/Button/Select 内容、开闭和焦点、子 popup 优先、隐藏/Modal/Spin 清理、有限视口及实际 glyph/quad/hit clips，通过 component 测试验证身份与草稿保留、同步卸载/重绑、受控拒绝及无焦点 defaultOpen。
- [x] 2.3 验证组合 wheel 与边界 fallback、child Select/Date/Time/Table、covered sibling 和外扩 focus decoration；通过新增组合测试及相关已有去重回归。
- [x] 2.4 交付 docs/popover.md 与可编译示例；Windows MSVC Ninja Multi-Config windows-msvc-headless Debug/Release、最多4 jobs，运行本 change 的针对性测试；通过 clang-format22 check、doctor、strict validate、diff check，记录实际证据并本地提交。

本次仅平台通用逻辑与 headless scene/input 合同；不设未实际运行的 native/GPU/Linux/system IME/AT 完成项，不改 Gallery。父任务负责集成与独立评审。

实际证据见 [windows-headless.md](evidence/windows-headless.md)：Debug/Release 去重组合各31/31，五份 golden 的15,372个旧叶子与33个 IIFE 保持，12个新 PE 程序的栈保留量均为默认1 MiB。

## 3. Astra R1 修复

R1 对 f3a900ec 提出6项 P2，重开上述相关验收，历史证据保留为历史结果。

- [x] 3.1 先执行失败回归，再拒绝最终不可渲染字体；验证 override/独立 seed/继承/祖先 update 的失败原子性和合法恢复。
- [x] 3.2 先执行失败回归，再按逻辑依赖同帧定位双向嵌套 Popover 与 Date/Time；验证打开、移动和视口变化。
- [x] 3.3 先执行真实 Pointer→Escape 失败回归，再修复父 picker 与已打开子 Popover trigger 的键盘归属；保留受控拒绝、IME、更深子层优先。
- [x] 3.4 先执行键盘打开父层→首焦点→Enter/Space 失败回归，再跳过代理 wrapper，保留其他合法 roving focus。
- [x] 3.5 先执行 reactive leave delay 失败回归，再重算保留方向的 pending deadline；验证 enter/leave 变长、变短、0、取消和销毁。
- [x] 3.6 先执行 hover pending→focus 失败回归，再让 focus 立即打开且无旧 deadline；验证失焦后正常关闭。

R1 修复实际证据见 [r1-review.md](evidence/r1-review.md)：6项先失败后通过，最终 Debug/Release 影响集合各44/44。

## 4. 父分支集成发布

- [x] 4.1 独立 Astra R2 复核后，保留已发布 086 Gallery 并合入 main；ROOT 正式 MSVC Debug 新构建组件/Theme/Gallery，执行 Popover、Date/Time 与必要共享回归、目录 contracts 和 inventory，记录边界后提交推送 main。
