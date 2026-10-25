# Tasks

## 1. 规划

- [x] 1.1 核对本地锁定参考与现有 popup/Theme 合同，完成 proposal/spec/design/tasks，经 doctor、strict validate 与 diff check 后本地提交。

## 2. 平台通用实现与验收

- [ ] 2.1 新增 typed API、开闭/触发/延时 model 与独立 Theme 完整链，验证 public API/model/Theme 测试；保留旧33个 IIFE、append-only identity，生成五份 goldens 并深比较旧字段。
- [ ] 2.2 实现 retained Popover host 与真实 Input/Button/Select 内容、开闭和焦点、子 popup 优先、隐藏/Modal/Spin 清理、有限视口及实际 glyph/quad/hit clips，通过 component 测试验证身份与草稿保留、同步卸载/重绑、受控拒绝及无焦点 defaultOpen。
- [ ] 2.3 验证组合 wheel 与边界 fallback、child Select/Date/Time/Table、covered sibling 和外扩 focus decoration；通过新增组合测试及相关已有去重回归。
- [ ] 2.4 交付 docs/popover.md 与可编译示例；Windows MSVC Ninja Multi-Config windows-msvc-headless Debug/Release、最多4 jobs，运行本 change 的针对性测试；通过 clang-format22 check、doctor、strict validate、diff check，记录实际证据并本地提交。

本次仅平台通用逻辑与 headless scene/input 合同；不设未实际运行的 native/GPU/Linux/system IME/AT 完成项，不改 Gallery。父任务负责集成与独立评审。
