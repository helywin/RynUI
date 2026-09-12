# 文本事件平台通用验证

日期：2026-09-12。范围：任务 3.1，尚不代表 session、平台 IME 或公开 Input 已完成。

- Windows 11，MSVC 19.51.36256 x64，`windows-msvc` / `windows-msvc-debug`，Ninja Multi-Config。
- 正式 Debug 全量增量构建通过。
- `ctest --preset windows-msvc-debug -R 'text_input_events|platform_input|input_dependency|focus|demo_frame' --output-on-failure`：9/9 通过。
- 新增 owned `String` / candidate snapshots、owner generation + session epoch、Unicode scalar selection 与未指定 range；测试包含无效 UTF-8 拒绝、空 composition、溢出 range、candidate selection/orientation 和入队后源对象变更。
- 单事件文本上限 1 MiB、候选项上限 128；batch 默认 4096 个事件 / 4 MiB 文本 payload，可配置更小预算。超限失败不修改已有队列；相邻 pointer move 继续合并，clear 保留 vector capacity。
- 现有无文本输入 owner 的 demo 显式忽略新增三类事件，未伪装为 Input 支持。
- OpenSpec doctor healthy，strict validate 10/10，`git diff --check` 通过。

此处只验证平台通用合同，不替代 Windows 原生 IME 或 Linux Wayland 验收。
