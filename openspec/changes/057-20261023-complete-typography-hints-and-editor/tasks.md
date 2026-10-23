# Tasks

## 1. 规划（平台通用）

- [x] 1.1 核对 Typography/Tooltip/TextArea 源码与固定 Ant Design 6.6.5 合同，创建 proposal/spec/design/tasks，运行 OpenSpec 1.14.0 doctor、全仓 strict validate 与 diff check 后以英文 Conventional Commit 提交规划。

## 2. 提示（平台通用）

- [x] 2.1 接入复制/成功/编辑与实际截断提示，补响应式文案/空文案、hover/focus、操作命中隔离、resize/展开、禁用/失活/销毁/idle 与 retained identity 回归和用户文档；Windows windows-msvc-headless Debug/Release 完整 CTest、格式与规格校验通过，记录实际 evidence 后独立提交。Debug 首轮98/99、修正新增测试时钟后失败项1/1复测通过；Release完整99/99，见 evidence/common-hints.md。

## 3. 多行编辑（平台通用）

- [x] 3.1 使用预建 TextArea 和 reactive typed autoSize，保留 Enter 提交/Shift+Enter 换行、IME/Escape/blur、controlled/方向/字体/maxLength/动态配置与生命周期；补普通 TextArea Enter 不变和 Typography 多行集成回归、用户文档与 Gallery 样例/目录，windows-msvc-headless Debug/Release 完整 CTest、Gallery 合同、格式与规格校验通过后记录 evidence 并独立提交。最终Debug/Release各99/99、Gallery 6/6，含Escape优先级与shell重入修复，见 evidence/common-editor.md。

## 4. 原生验收

### Windows

- [ ] 4.1 windows-msvc Debug/Release MSVC x64 构建和受影响 native CTest，真实 D3D12/DXIL 窗口验证浅/深色提示、复制反馈、截断/展开/resize、多行编辑/提交/取消、系统与 1/1.25/1.5/2 scale、idle/dispose，核对截图/readback、字体/driver/退出码/log/hash，独立记录并提交 Windows evidence。

### Linux

- [ ] 4.2 在实际 Linux 机器 linux-native Debug/Release 构建与受影响 native CTest，完成对应 Vulkan/SPIR-V/系统字体/输入/DPI/resize 窗口验收，独立记录并提交 evidence；不重复 common，也不使用 Windows/WSLg 结果替代。
