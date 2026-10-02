# Windows 原生实施与验收

2026-10-02，实际环境为 Windows 11 专业工作站版 10.0.26300、MSVC 14.51.36231、Ninja Multi-Config。使用 `windows-msvc-debug` / `windows-msvc-release` configure、build 和 test presets，真实 GPU 驱动为 Direct3D 12，shader 格式为 DXIL。

- Debug、Release 全部 native 目标构建通过。Debug 受影响 CTest 12/12（120.86 秒），Release 9/9（0.65 秒）；涵盖 R8 source/SDL packing、texture batch、atlas resources、font、scene/frame、lifetime、组件/allocation、生成及部署 shader 合同。
- 两种配置分别完成真实 Gallery resize、Selection 2.0 scale、Typography 系统 scale 1.25 和显式 scale 2.0，共 8 次运行，全部退出码为 0。Gallery 实际 resize 到两种客户区大小；Selection 记录 31 个输入事件，keyboard、pointer、disabled 阻断均通过；Typography 包含剪贴板、ellipsis、编辑、Link、Divider 及 GPU 回读。
- `windows/runs.json` 保存每次运行、最终可执行文件 SHA256、42 张 PNG 的 SHA256；`verify_evidence.py` 验证全部 8 次运行、42 张图片与当前 Debug/Release 二进制一致。Selection 与窄窗口 Gallery 代表截图人工检查通过。
- `compare_typography.py` 对比本次和 037 已验收的 36 张 Typography GPU 回读图，尺寸与规范化 RGBA 像素全部一致。比较结果见 `windows/pixel-comparison.txt`，不依赖 PNG 压缩字节是否一致。
- SDL 设备创建过程中实际查询 R8 sampled texture 能力并验证 manifest，运行成功确认当前设备满足此项。资源输入上限仍是接口可接受输入的上界，不是可用显存或所有尺寸创建成功的承诺。

构建、CTest、应用日志、截图、运行脚本、hash 校验与像素比较原始证据保存在 `windows/`。平台通用合同已另行记录在 `common-contracts.md`，不重复验收。本记录不声称测量 GPU 时间、手工 IME 或 Linux 行为；Linux 原生验收仍待实际环境执行。
