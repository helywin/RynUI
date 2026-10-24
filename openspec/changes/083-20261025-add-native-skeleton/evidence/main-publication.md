# 083 main 集成与发布

2026-10-25，父分支基线 `7474a10a71430b6a61da959d439897b0605c0724`。使用显式 merge 合入 `a55f22e4ab3bf0583e611b685217901530036866`，无冲突。该分支完整保留 Astra R1 CLEAN 的 `9a83644` 生产源码，并已合并 `22ee615` Gallery。父基线相对 `22ee615` 仅增加 079 验收文档。

## 源码与范围

`git diff --quiet a55f22e -- src include tests CMakeLists.txt CMakePresets.json design-tokens` 通过；父集成没有改写已审查实现。更新中英文 README、completion、Skeleton 的 partial support overlay，并重新生成离线 catalog。首版保持静态，无 active/shimmer、Image/Node 或新交互 Gallery。

`verify-baseline.py` 与父集成比较脚本通过：五份 goldens 的 **15,047** 个旧 scalar typed values、**31** 个旧 Theme IIFE 原文与顺序保持；TokenIdentity 仅追加。新构建的 Theme algorithm 测试实际解析结果与提交 goldens 一致，没有从过时二进制生成文件。

## ROOT 本轮实际验证

正式 `windows-msvc-debug`，MSVC x64 / Ninja Multi-Config，最多 **4 jobs**。忽略目录辅助脚本 `out/wrapup-msvc.ps1 -Phase skeleton` 执行：

1. `cmake --build --preset windows-msvc-debug --parallel 4 --target`：Gallery、frame、reference catalog，Theme algorithm/runtime，以及 Skeleton model/public API/component/Theme/integration 共 10 个目标；实际 **125** 个编译/链接动作，退出 0。
2. 精确选择 Skeleton 的 11 项、Theme algorithm/runtime 两项、目录与源码 contracts 八项，CTest **21/21**，real **1.08 s**，退出 0；JUnit 在 `out/wrapup-skeleton-ctest.xml`。
3. 新 frame binary `--inventory-only` 退出 0：**334 IDs、93 navigation、350 Buttons、1380 interactions、647 visible、147 Inputs、286 live**。Form3/Item6、Date2/Time2、Breadcrumb2/Steps1 保持。
4. clang-format 22.1.3 检查 **659** 个文件，零失败；doctor healthy，strict **82/82**，staged/unstaged diff checks 通过。

原始日志：`out/wrapup-skeleton-{build,ctest,inventory}.log`。原有四条 frame MSVC warnings 及 VSDevShell 的 vswhere 提示保留；实际编译、链接和测试均成功。

组件原实施的 MSVC headless Debug/Release 各 **51/51** 和独立 Astra R1 的各 **19/19** 已在对应工作区完成；本轮没有重复这些矩阵。先前 079 的完整 frame **9/9** 和 D3D12/DXIL 原生启动属于固定 `22ee615`，不冒充 Skeleton 新 GPU 验收。组件人工视觉、GPU/DPI、Linux、系统 IME、AT 和交互 Gallery 仍待独立完成。

发布只推送 main，并在推送后检查远端 SHA；不推送 `t3/*`，不创建 PR 或 archive。
