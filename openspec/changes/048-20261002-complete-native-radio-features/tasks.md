# Tasks

## 1. 动态值、组合与键盘（平台通用）

- [x] 1.1 实现兼容 String 的类型值、reactive options、最近 typed Group、稳定身份/删除回滚、单一 Tab 入口/方向键、ref/autoFocus/onClick；补 docs/radio.md 与 public API/生命周期/旧用法合同。在 Windows `windows-msvc-headless` Debug/Release 构建并通过 radio_component、radio_features、radio_public_api、selection_component、selection_controls_public_api、focus_order/state/lifecycle；运行格式、OpenSpec 与 diff 检查后提交。实际 Debug 8/8（1.59s），Release 8/8（1.08s），407 自有源格式检查通过。

## 2. 按钮、主题与反馈（平台通用）

- [ ] 2.1 实现 RadioButton/optionType、outline/solid/三尺寸/block/H/V/RTL、共同相邻形状、独立 Radio Theme 与有限 wave；补相邻角/边界/焦点、主题局部更新/golden 和空闲 deadline 合同与文档。HEADLESS Debug/Release 通过受影响 Radio/Theme/共同表面/rounded_effect/renderer contract；格式、OpenSpec 与 diff 检查后提交。

## 3. 集成（平台通用）

- [ ] 3.1 增加 Gallery 原生样例、更新目录与收尾清单；运行生成器 self-test/check、HEADLESS Debug/Release 完整 CTest 与文档检查，记录实际 preset/结果后提交。

## 4. 平台集成

### Windows

- [ ] 4.1 `windows-msvc` Debug/Release 构建和受影响平台 CTest；真实 D3D12 窗口验证鼠标/键盘/焦点、动态删除 capture、outline/solid/连接角、主题、系统字体、RTL、resize、系统及 1/1.25/1.5/2 缩放、有限反馈/空闲/销毁；保存程序日志、PNG/hash/EXE hash 和复现脚本，并完成 Gallery Debug/Release 原生烟测后独立提交。

### Linux

- [ ] 4.2 在实际 Linux 机器完成本 change 的原生窗口/GPU/shader/system-font/input/DPI 与 resize、反馈空闲/销毁验收，保存对应证据并独立提交；不重复已通过的平台通用合同。
