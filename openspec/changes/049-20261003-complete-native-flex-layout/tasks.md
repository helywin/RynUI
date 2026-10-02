# Tasks

## 1. 换行和方向（平台通用）

- [ ] 1.1 实现兼容 bool 的 typed WrapReverse、LTR/RTL、物理 Left/Right justify 与原生等价值映射，保留声明/focus 顺序和局部 placement；加入 docs/flex-space.md 与 public API、H/V/RTL/反向换行、非法值、retained/销毁合同。Windows `windows-msvc-headless` Debug/Release 构建并通过 LayoutEngine/LayoutStyle/Flex/Space/public API 的受影响 CTest；格式、OpenSpec 与 diff 检查后提交。

## 2. 基线和默认值（平台通用）

- [ ] 2.1 传递真实 Text/Typography/Input 基线、共同容器/控件内部偏移、line ascent/descent 和 align-self Baseline；修正默认 Stretch、迁移需要旧 Start 的调用。加入混合字号/多行/缓存/字体变更/margin/控件/零约束合同，HEADLESS Debug/Release 受影响布局/文本/组件测试通过；更新文档，格式/OpenSpec/diff 检查后提交。

## 3. 集成（平台通用）

- [ ] 3.1 补 Gallery 基线/WrapReverse/RTL 样例，同步目录和收尾清单；生成器 self-test/check、HEADLESS Debug/Release 完整 CTest、文档与格式/OpenSpec/diff 检查通过，记录实际 preset 和结果后提交。

## 4. 平台集成

### Windows

- [ ] 4.1 `windows-msvc` Debug/Release 构建、受影响原生 CTest 与 Gallery 真窗口 smoke；实际 D3D12/DXIL/system fonts 窗口验证系统及 1/1.25/1.5/2 render scale 的文字基线/控件、H/V/wrap-reverse/RTL、resize/指针命中、局部更新/idle/销毁，保存日志、PNG/hash/EXE hash 与复现脚本，检查后独立提交。

### Linux

- [ ] 4.2 在实际 Linux 机器完成本 change 的窗口/GPU/shader/系统字体/input/DPI、resize 与 idle/销毁验收，保存证据并独立提交；不重复平台通用合同。
