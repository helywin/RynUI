# 构建边界验收

实际平台：Windows 11、MSVC 19.51 x64、Ninja Multi-Config。2026-10-02。

- 默认 `windows-msvc-debug` 构建通过；13/13 受影响资源、输入 GPU、场景与 shader ABI tests 通过。日志：`out/035-native-build.log`、`out/035-boundary-native-tests.log`。
- `windows-msvc-headless-debug` 实际编译生产 Core/共同 renderer，18 个已有 CPU/组件 fixtures 与 1 个真实构建图/负向边界 fixture 共 19/19 通过。日志：`out/035-headless-configure.log`、`out/035-headless-build.log`、`out/035-headless-tests.log`。
- 无效 `RYNUI_SHADERCROSS_EXECUTABLE=Z:/missing-shader-tool.exe` 不影响 HEADLESS 配置/编译；guard 检查实际 compile_commands 与 Ninja 图无 SDL、shadercross、libdecor、系统字体源码。
- 非法宿主、未知/空 renderer、HEADLESS + SDL_GPU、Core 内部 SDL include、经中间 target 的原生链接均被独立负向配置 fixture 拒绝。
- 首次文本依赖下载停滞后复用本地已锁定 FreeType/HarfBuzz/utf8proc 源码，仅作为 FetchContent source override；仍在 HEADLESS binary tree 独立编译。没有复用 SDL 或 shader 工具。验证字体仍使用锁定字体文件。

首次共同 target 漏掉公开 include 路径，MSVC 编译明确失败；补齐路径后重建通过。HEADLESS 边界 fixture 首次使用复数 component 目录断言失败，改为实际 `component/` 并同步守卫后通过。这两次失败不作为成功证据。

本阶段尚未实现 Recording、共同事务或 callback pump；不代表 Linux、新 GPU backend 或 GPU 视觉验收。
