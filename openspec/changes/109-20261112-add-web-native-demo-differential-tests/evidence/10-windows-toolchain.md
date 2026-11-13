# Windows 锁定工具链准备

2026-11-14，Windows x64。父集成 checkout 实际执行，原始准备记录、归档与日志只在 ignored `out/`。

- 显式 BUNDLED 准备官方 CPython **3.14.0 / cp314-win_amd64** 与 Pillow **12.0.0**；核完整文件、路径绑定与来源，实际 import 及 PNG/BMP 往返通过。没有系统安装、pip 或隐式解释器回退。
- 使用已核 SHA256 的显式归档输入与 `--offline`，在 fresh 输出目录完成 Node **22.21.1**、pnpm **10.23.0**、固定 AntD/React/Playwright 及平台 esbuild 闭包的真实 frozen-lockfile/ignore-scripts 安装；Chromium **141.0.7390.37 / revision 1194**。归档导入不复用另一 checkout 的安装目录。
- CPython 3.14 的 Windows preparation 自动控制 **12/12**，独立 comparison dependency 控制 **2/2**。覆盖 fresh/offline、文件/ABI/来源错配、外部覆盖与禁止 fallback；这些是 CPU/准备合同，不计 GPU 或 paired pass。
- runner 自动准备 Pillow 时显式传播 offline；显式 comparison-site 仍须完整核验。文档入口为 `comparison_runtime.prepare_runtime(..., mode='BUNDLED')`，返回的解释器/flags 执行 `tools/demo_reference_web/bootstrap.py --mode BUNDLED --offline --output <fresh out>`。

OpenSpec doctor/strict 与 diff 检查通过。浏览器字体/组件动作、native D3D12、pilot 校准及实际三例红绿另行验证；本提交不关闭 5/11 平台 checkbox，不把准备成功描述为视觉等价。
