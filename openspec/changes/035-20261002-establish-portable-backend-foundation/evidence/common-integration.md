# 平台通用集成验收

日期：2026-10-02。实际机器：Windows，MSVC x64 14.51.36231，Ninja Multi-Config。所有命令通过 Visual Studio 18 Community 的 `VsDevCmd.bat -arch=x64 -host_arch=x64` 进入工具链；不是 MinGW 或跨平台运行结果。

| 配置 | build | CTest |
| --- | --- | --- |
| windows-msvc-headless / Debug | exit 0 | **21/21**，4.34s |
| windows-msvc-headless / Release | exit 0 | **21/21**，3.39s |
| windows-msvc / Debug | exit 0 | **240/240**，164.40s |

共同测试运行实际组件、FT/HB/utf8proc 与锁定字体、共同 resources、Recording 与 frame pump；compile commands/Ninja 检查无 SDL/shadercross/libdecor/default-font。负向 fixture 拒绝未知/空/不支持 backend、Component SDL include、Input Windows include、未来 renderer include、条件 generator expression 包装的传递 SDL link。默认完整 suite 包含真实 source/ABI/shader/dependency 合同，不能替代原生窗口验收。

完整日志保存为 common-full-ctest.txt、common-headless-debug.txt、common-headless-release.txt；build 日志在 `out/035-final-headless-debug-build.log`、`out/035-headless-release-build.log`、`out/035-final-native-debug-build.log`。

首次完整 suite 239/240：旧 Button frame fixture 依赖动画 dirty 无意产生的 immediate wake，要求 deferred 自动紧接重试。改成检查 pending revision/dirty 保留、无立即 request，再显式 request 同 timestamp 重试；这是新 deferred 合同的测试迁移。条件传递 link 负向 fixture 也曾暴露 guard token 解析漏掉 generator expression 的 `:middle`；修正解析后 Debug/Release 的实际负向验证通过。

正式架构/renderer ABI、研究当前/未来范围、README 和 agent 规则已更新。无新 OS、Web/移动产物、运行时 selector、自动 device-loss 或第二真实 GPU backend。Linux 原生 checklist 保持 pending，共同逻辑不重复要求 Linux 验收。
