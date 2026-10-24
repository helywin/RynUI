# Tasks

## 1. 平台通用：可操作样例

- [x] 1.1 在已集成的 073 上追加 Empty/Result 原生样例、稳定标识、目录与 API 文档；增加聚焦 Recording 测试并验证按钮驱动描述/图示/状态、实际 Input 草稿保留、身份和默认/Dark/Compact 宽窄视口 glyph/图示裁剪，运行 generator/contract、格式检查、doctor、strict validation 与 diff check。

## 2. 集成验收

- [x] 2.1 在 Windows MSVC windows-msvc preset 实际构建 Gallery，运行完整 token_gallery_frame 与八项 Gallery contracts，并运行原生 --smoke 检查启动、主题切换及正常退出；记录完整 inventory、准确命令与平台边界，更新完成清单。原生 smoke 仅为 Windows 启动证据，不代表组件人工视觉、系统 IME 或 Linux 验收。

2026-10-25：15c356c 的聚焦、八项合同及完整 Gallery 9/9（frame 226.37 秒）已实际通过，但 Astra R1 发现目录只能到参考卡片，尚不能直达 Empty/Result 实际样例，未满足 spec 的目录定位要求。1.1 恢复未完成，保留原成功证据；补通实际目录操作并验证后再勾选，不把旧测试通过等同完整实现。

同日 R1 修复：原目录按钮已定位实际 Empty/Result 样例；真实点击→导航请求→生产共用锚点/跳转→实际按钮及 Input 操作的聚焦测试通过，覆盖三个 Theme 与两种宽度。文档 model/viewport 与八项合同共 10/10 通过；格式、doctor、strict 76/76、diff check 通过，因此重新完成 1.1。2.1 等待本次导航修复后的完整 Gallery 与原生 smoke。

最终收口：`5dc9a22` 源代码完整 Gallery **9/9，337.39 秒**（frame 336.91 秒）通过，原生 D3D12/DXIL、1.5 倍缩放 smoke **7 frames、exit 0**；Astra R2 固定此提交复核完整范围，确认 R1 已解决且无可操作 P1/P2。076 为 **2/2 完成**，平台边界见验收记录。
