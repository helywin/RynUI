# Rate main 集成验收

2026-10-26：main `34eefb82c57f3d3bdf687db477aef9de9247b4b3` 与 Rate worker `48d4ac03ac44260c3c888af0147c73203bbb8490` 显式合并。worker 已经独立 Astra R3 完整审查，未发现可达 P1/P2；其 existing-binary 复验不代替本记录的 main 新构建。

## 合并与保留

Theme、窗口服务和 CMake 的并行追加冲突保留双方接入：Avatar 在前、Rate 在后。Rate 自有 API、host/model、Theme resolver、测试和 `docs/rate.md` 与已审 worker blob 一致；父 `examples/`、`gallery/` 与 `tests/token_gallery_frame_tests.cpp` 完整保持。主 README 与组件进度追加实际能力，未将 093 规划计为 Gallery 实现。

五份金样由实际新构建的组合 Theme 程序生成，Debug/Release 字节一致。`verify-main-theme.py` 对双方父提交作深比较：main 的 **16,727** 个旧 typed leaves、Rate worker 的 **16,662** 个旧 typed leaves 均保持（仅排除顶层 identity）。保留 main **37** 个 IIFE 原文/顺序，追加 Rate **1** 个；TokenIdentity 保留 **198** 个旧 ordinal，仅追加 Rate **2** 个。

共享命中路径保持窗口层全窗语义；普通 document interaction 仍叠加 document clip，缓存 source clip 与局部刷新行为不变。notice/Timeline 裁剪与关闭转换回归包含在本轮组合集合中。

## 实际命令与结果

Windows VS MSVC x64、`windows-msvc-headless`、Ninja Multi-Config、BUNDLED，最多 2 jobs。通过 VSDevShell 后执行：

```text
cmake --preset windows-msvc-headless
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_rate_model_tests rynui_rate_theme_tests rynui_rate_public_api_tests rynui_rate_component_tests rynui_rate_lifecycle_tests rynui_rate_font_limit_tests rynui_rate_focus_order_tests rynui_rate_review_tests rynui_avatar_theme_tests rynui_avatar_component_tests rynui_portable_theme_algorithm rynui_portable_theme_runtime rynui_popover_integration_tests rynui_timeline_review_tests rynui_notice_review_tests
out/build/windows-msvc-headless/tests/Debug/rynui_avatar_theme_tests.exe out/rate-main-goldens
ctest --preset windows-msvc-headless-debug --parallel 2 --output-on-failure -R "^rynui\.(rate_.*|avatar_.*|portable\.theme_(algorithm|runtime)|popover_(wheel|children|covered|clips)|timeline_review_(fill|document_hit)|notice_review_clip_contract)$"
```

Release 使用同一 target/filter 和 `windows-msvc-headless-release`；主题程序另输出 `out/rate-main-release-goldens`，与 Debug fresh 文件逐字节比较。

```text
python openspec/changes/090-20261026-add-native-rate/evidence/verify-main-theme.py 34eefb82c57f3d3bdf687db477aef9de9247b4b3 48d4ac03ac44260c3c888af0147c73203bbb8490 out/rate-main-goldens
python scripts/format-code.py --check --clang-format "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe"
openspec doctor --json
openspec validate --all --strict --no-interactive
git diff --check
```

全部 exit 0：Debug **29/29**，CTest real **4.62 秒**；Release **29/29**，CTest real **2.39 秒**。包括 Rate 11 项、Avatar 9 项、共享 Theme 2 项、Popover 4 项、Timeline 2 项、notice 1 项。Release 前段构建因宿主会话中断而停止，确认无存活构建进程后续建；本轮实际续编译/链接 **199.5761721 秒**，不是完整从零构建耗时。没有重跑已通过的 Debug 测试。

16 个实际 main Rate Debug/Release x64 PE 的 stack reserve 均为 **1,048,576 bytes**。clang-format 22.1.3 检查 **748 files、0 failures**；显式 OpenSpec **1.14.0** doctor healthy、strict **91/91**。保留已有 MSVC warning 和 VSDevShell 的 `vswhere.exe` 提示，没有扩大栈或升级全局工具。

原始日志：`out/090-main-{debug,release}-{build,test}.log`、对应 CTest XML、`out/090-main-debug-{configure,goldens}.log`、`out/090-main-doctor.json`、`out/090-main-strict.log`。忽略目录的 `out/090-main-msvc.ps1` 只设置环境、输出日志与计时。

## 验收边界

本记录证明上述源码合并与本机 HEADLESS 合同；未执行 Rate 交互 Gallery、完整 Gallery/native smoke、组件 GPU/人工视觉、Linux、系统 IME 或 AT 验收。093 仍为规划阶段，须在其 Core 依赖全部通过后另行实施。只推 main，不推 `t3/*`，不创建 PR 或 archive。
