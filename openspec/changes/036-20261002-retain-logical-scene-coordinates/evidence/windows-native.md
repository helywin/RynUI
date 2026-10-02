# Windows 原生验收

scope: windows-native
status: passed
日期：2026-10-02。环境：Windows 11 专业工作站版 10.0.26300，MSVC x64 14.51.36231，Ninja Multi-Config，SDL GPU Direct3D 12 / DXIL，Win32 窗口。

## 构建与集成测试

- `windows-msvc-debug` 与 `windows-msvc-release` 全部目标构建成功。最终 Selection 验收修正后的增量构建日志见 [Debug](windows/debug-build.txt)、[Release](windows/release-build.txt)。
- logical scene 实现的 native Debug 完整 CTest **241/241**、179.71 秒，包含 Windows 字体、shader、平台生命周期及 renderer 检查，见 [native-debug-ctest.txt](native-debug-ctest.txt)。最后只修改 Gallery 的 Selection 自动验收定位逻辑；相应 source、Gallery 与 viewport 合同复验 **4/4**、0.16 秒，见 [gallery-acceptance-checks.txt](windows/gallery-acceptance-checks.txt)。
- 最终 Release 的 `windows_libdecor_isolation`、`default_font_chain`、`platform_lifecycle`、`frame_renderer`、`generated_shaders`、`deployed_shaders` **6/6**、0.45 秒，见 [release-checks.txt](windows/release-checks.txt)。

## 真实窗口与图像

[validate_native.py](windows/validate_native.py) 在本机启动最终 Debug/Release Gallery 可执行文件；共 **8 次运行全部 exit 0**：

| 每种配置的模式 | 实际验证 |
| --- | --- |
| Gallery `--smoke` + Win32 resize | 窗口外框 1200×840 → 900×700；实际 client 1182×793 → 882×653；两次 resize 后均抓取真实 client 图像并完成 smoke |
| Typography，系统 scale | `system_display_scale=1.25`、`render_scale=1.25`；clipboard、ellipsis、edit、link、divider 全部 passed |
| Typography，override scale 2.0 | `render_scale=2`；上述交互全部 passed，并保存 GPU readback 图像 |
| Selection，override scale 2.0 | 五个自动交互阶段，31 个归一化输入事件；keyboard、pointer、blocked 全部 true；Switch/Checkbox/Radio 截图 |

Selection 原有验收滚到 `maximum_offset`；追加 Typography/Divider 后，这会把 Selection 控件留在屏外。本阶段改为依据实际 mounted controls 的 bounds 居中滚动，并在每次指针派发前检查目标中心位于 document viewport 内，避免屏外禁用控件的空操作被误当成验证。最终脚本等待第五阶段完成再抓图。

已检查最终 wide/narrow resize、Selection、Typography editing 与 Divider 图像：可见内容与裁剪边界对应，字形、圆角、focus、选中/半选/禁用状态及文本装饰位置正常。此处是自动窗口交互加图像检查，不是手工原生 IME 验收或 GPU 性能测量。

实际系统字体为 Segoe UI Variable Text、Segoe UI strong/italic、Cascadia Mono；Gallery CJK fallback 为 Microsoft YaHei UI。现有字体解析对 weight 500 使用 regular fallback，日志保留该诊断。

[runs.json](windows/runs.json) 记录每次运行的配置、exit code、实际尺寸、当前可执行文件 SHA256 与截图 SHA256。共 **42 个 PNG**（36 个 Typography、4 个 resize、2 个 Selection）。[verify_evidence.py](windows/verify_evidence.py) 检查全部图像及两种当前二进制哈希，结果见 [hash-verification.txt](windows/hash-verification.txt)。

## 文档与平台边界

`openspec doctor --json` healthy；`openspec validate --all --strict --no-interactive` 36/36；`git diff --check` 通过。Linux 原生任务 5.1 没有本机证据，保持未完成；macOS、移动端、浏览器与第二真实 GPU backend 不属于本阶段已实现能力。
