# Tasks

## 1. 换行和方向（平台通用）

- [x] 1.1 实现兼容 bool 的 typed WrapReverse、LTR/RTL、物理 Left/Right justify 与原生等价值映射，保留声明/focus 顺序和局部 placement；加入 docs/flex-space.md 与 public API、H/V/RTL/反向换行、非法值、retained/销毁合同。Windows `windows-msvc-headless` Debug/Release 的 LayoutEngine/LayoutStyle/Flex/Space/public API CTest 各 10/10（0.59s/0.46s）；408 自有源格式、OpenSpec doctor/strict 49/49 与 diff 检查通过。

## 2. 基线和默认值（平台通用）

- [x] 2.1 传递真实 Text/Typography/Input 基线、共同容器/控件内部偏移、line ascent/descent 和 align-self Baseline；修正默认 Stretch、迁移需要旧 Start 的调用。混合字号/多行/缓存/字体变更/margin/控件/零约束与嵌套拉伸 Button 合同通过；`windows-msvc-headless` Debug/Release 受影响 CTest 各 22/22（7.64s/5.26s），Windows 原生 Flex/Gallery frame 2/2 与 Debug 真窗口 smoke 通过。文档、格式/OpenSpec/diff 检查见共同证据。

## 3. 集成（平台通用）

- [x] 3.1 补 Gallery 基线/WrapReverse/纵向 RTL/默认 Stretch 四组样例，同步目录和收尾清单，Flex 原生功能 implemented。生成器 write/self-test/check 通过；`windows-msvc-headless` Debug/Release 完整 CTest 各 65/65（90.26s/15.97s），原生 Gallery frame 通过（22.45s）；409 自有源格式、OpenSpec doctor/strict 49/49 与 diff 检查通过。Gallery 102 IDs、118 live samples、59 Theme scopes。

## 4. 平台集成

### Windows

- [x] 4.1 `windows-msvc` Debug/Release 构建、受影响原生 CTest 各 10/10（28.26s/9.51s）与默认栈 Gallery 真窗口 smoke 通过；实际 D3D12/DXIL/DefaultFontChain 系统字体的 D/R 系统及 1/1.25/1.5/2 render scale 共 10 次窗口、130 PNG/readback hashes 与 EXE hashes 全部通过。基线/控件、H/V/wrap-reverse/RTL、1420x900 resize、3 次规范化指针事件/1 click、纯 placement 的 measure/shape 稳定、3 idle polls 无 deadline、新帧或提交、销毁零资源通过；410 自有源格式与 OpenSpec/diff 校验通过，见 evidence/windows。

### Linux

- [ ] 4.2 在实际 Linux 机器完成本 change 的窗口/GPU/shader/系统字体/input/DPI、resize 与 idle/销毁验收，保存证据并独立提交；不重复平台通用合同。
