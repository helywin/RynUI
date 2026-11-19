# Pilot 的真实输入与回调对照：Windows

2026-11-19，`windows-msvc-release` hidden D3D12/DXIL 与 Chromium 141.0.7390.37，实际 DPR 1.5，固定七字体链。

- Button 的实际 slot 输入为 `默认`，官方渲染的文字观测保留 `默 认`。输入比较仅识别锁定 Button 对符合条件的两个汉字插入的一个显示空格。
- Input 的 Space 观测只展开官方组件的 `SpaceContextProvider`，保留实际 Item 分支和顺序。未知布局容器仍须由原结构校验处理。
- 执行 Button、Input、Select 的真实输入路径后，以 `runner_evidence.effective_inputs` 对照两侧记录。三例共 **17** 个检查点的实际输入与独立语义断言通过，Button、Input 的回调序列一致。重建的浏览器 bundle 与独立控制 bundle 的 **659** 个官方源码记录逐项相同，pilot 定义检查 **11/11**，Button/Space 定向浏览器回归 **2/2**。
- Select 回调序列有差异。首次 `open-change` 在 Web 的 `open-down` 和 native 的 `open-up` 触发。`virtual-close-down` 和 `async-close-down` 的闭合、搜索清空回调也有差异。原始回调保留，差异交由配对行为验收处理。

4.4 的 case 定义、源码、覆盖与实际输入已验证，pilot 账本的 32 条定义标记为 ready。配对判定还须通过完整回调合同，并使用有效的独立校准 profile。原始截图与报告保存在 ignored `out/`。
