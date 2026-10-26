# Windows 原生资源与独立目录验收

## 范围与实现

基线为 main `4ce0bedb7bc432d4aec07482d3b13d624d260da1` 加本次实现。用户的 qrc 是类比；最终使用标准
Windows `.rc` / `RCDATA`，没有新增清单语言、XML parser、公开资源包 API 或 Qt 依赖。
12 个 shader 直接链接到 executable，两个 SDL renderer 从模块拥有的只读 bytes 创建 GPU shader。
非 Windows 保留固定 shader 集的 C++ 数组实现；本轮没有 Linux 原生验收。

普通 text/button/layout/Gallery 默认请求系统字体，不再指定或复制验收字体。固定字体验收通过显式目标
`rynui_token_gallery_acceptance_fonts` 准备；该目标不属于 ALL。两份复制结果与锁定输入 SHA256 一致。
专用 rounded acceptance 和其他显式字体配置保持。

## 实际构建与检查

本机 Windows、Visual Studio MSVC、Ninja Multi-Config；使用正式 presets，build 最多 2 jobs。
先完成全部指定目标的新构建，再对发现的问题作必要增量重建。保存的 build 日志是最终增量构建，
不冒充 clean build 日志；源码、最终程序和相关构建记录索引见 `windows/`。

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug --parallel 2 --target rynui_minimal rynui_text_demo rynui_button_demo rynui_layout_demo rynui_token_gallery rynui_rounded_acceptance rynui_embedded_shader_tests rynui_default_font_chain_tests
cmake --build --preset windows-msvc-release --parallel 2 --target rynui_minimal rynui_text_demo rynui_button_demo rynui_layout_demo rynui_token_gallery rynui_rounded_acceptance rynui_embedded_shader_tests rynui_default_font_chain_tests
```

两个配置各 **7/7 PASS**：`embedded_shaders`、`generated_shaders`、`default_font_chain`、
`glyph_shader_contract`、`quad_shader_contract`、`rounded_effect_shader_contract`、`text_example_contract`。
CTest 使用对应 preset、`--parallel 2 --output-on-failure` 和以上名称的精确正则；原日志与 JUnit 均保存。
HEADLESS 正式 preset 的 configure 守卫及 Debug/Release `rynui_renderer_common`、`rynui_components` 构建通过，
没有为 HEADLESS 引入 RC、Windows 资源读取或 SDL renderer。

内嵌比较覆盖全部 12 个输入，合计 **44,888 bytes**，逐字节一致，重复读取地址稳定，未知资源明确失败。
真实改变生成的 `quad.vertex.dxil`、只重建测试 executable，日志确认 RC 与 linker 重跑，
读取 **44,910 bytes** 与改变后的输入一致；在 finally 恢复原文件并重建，重新得到 **44,888 bytes** 一致。
原 shader SHA256 恢复为 `701b28d99314f27af52bd82cdced782d36d87a099af3144540426ffd8d428f31`。
该探针只用于资源依赖，不把修改后的 shader 送入 GPU。

另用 CMake 生成非 Windows 分支的固定数组，并在本机 MSVC 以 C++20 编译同一 byte-comparison 程序，
12 项全部一致。这只验证生成器和 C++ 数据合同，不是 Linux/Vulkan 验收。

## exe 单独搬移与实际 GPU

每个目标各自复制到一个新目录，目录内只有该 exe；工作目录是另一个空目录。
没有复制 shaders、fonts、源代码或 shadercross。实际 `--smoke` 结果如下：

| 配置 | minimal | Token Gallery |
|---|---|---|
| Debug | exit 0，2.047 s | exit 0，67.453 s |
| Release | exit 0，2.031 s | exit 0，11.531 s |

四次均为 `gpu_driver=direct3d12`、`shader_format=DXIL`，stderr 为空。
Gallery 实际 `font_source=system`，字体来自 `Segoe_UI_Variable_Text,Microsoft_YaHei_UI,Segoe_UI`，
并完成 glyph 绘制与 7 个 frame samples。原始 stdout、stderr、命令路径、cwd、目录内容、程序 SHA256、时间戳
均在 `windows/{debug,release}-manifest.json`。检查的六个 PE 均保持默认 **1 MiB stack reserve**。

这证明本机不再依赖旁置 shader/默认字体目录；不扩大为新机器 VC runtime 分发、人工视觉、Linux、系统 IME 或 AT 验收。

## 发现、修复与审查

1. 旧 minimal 仅复制 exe 后实际报 `Unable to open shader`，保留在本机
   `out/095-relocation-before-1793024363671/`；这是旧二进制失败证据，不算新实现验证。
2. 初版 RC 名称错误地包含引号。真实 byte-comparison 失败，Win32 资源枚举确认引号属于名称；
   改为无引号的资源标识符，路径仍带引号。最终两配置 12 项比对及 GPU 创建通过。
3. 搬移脚本最初错误地要求 minimal 打印 Gallery 才有的 `exit_code=0` 字段；minimal 进程本身已经 exit 0。
   修正探针后使用进程返回码，并保留 Gallery 的 telemetry 断言。
4. 新 Gallery Debug 实跑暴露两个深栈路径：挂载中的大型默认 ThemeConfig 临时对象，以及同步主题通知中的多余按值副本。
   CDB 日志保存了 `0xC00000FD` 和实际调用栈。只调整示例：ThemeProps 放堆上，8 个默认配置借用已有静态 const 默认值，
   set_theme 的两个中转参数使用 const 引用；signal 仍复制持有配置，没有修改 Theme Core、GPU ABI 或 PE 栈。
   最终完整 smoke 的主题更新、原 telemetry 和实际绘制通过。
5. Astra R1 找到专门验收字体准备遗漏；补充上述显式 target、实际 hash 检查和构建文档。
   全新 Astra xhigh/default R2 **CLEAN**，完整复核资源与字体路径、Props 持有、const 引用生命周期和原通知顺序。
   R2 是只读源码审查，实际构建与运行证据由本父任务产生，两者不混称。

Theme 实现及其解析 IIFE、ordinal/golden、Core/common、shader HLSL/GPU ABI 均未修改；Gallery 保留原 IDs 与功能，
只处理本次真实启动所需的栈问题。格式、显式 OpenSpec 1.14 doctor/strict 与 diff 检查另见收口记录。

最终检查：clang-format **22.1.3，761 文件、0 failures**；显式 OpenSpec **1.14.0** doctor healthy、
strict **93/93**；`git diff --check` 通过。原始结果和日志／manifest 索引随 `windows/` 保存；
该目录保留原始字节，避免 Git 换行转换破坏 SHA256 索引。

## 停止范围

用户要求本功能完成后停止。本轮不合并后续 AutoComplete、Tree、093 Gallery，不增加新组件或新 change。
Linux 平台任务保持未勾选；本轮不创建 PR 或 archive，只提交并推送 main 后核对远端 SHA。
