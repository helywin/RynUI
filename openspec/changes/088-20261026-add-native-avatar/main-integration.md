# Avatar main 集成验收

2026-10-26，ROOT 将 main `83d26e7a16a0590c2d52867296a20dd3cf4aee78` 与独立 Astra R2 已通过的 worker `75595ccd9e8b60b5b84eb8e4db785b3c4c9a3f77` 显式无冲突合并，按两个父提交记录。R2 完整首版审查未发现可达 P1/P2，worker existing-binary 的 Debug/Release 各 5/5 不能代替下面的 main 新构建。

Git 逐路径比较确认：`include/`、`src/`、`design-tokens/`、`tests/CMakeLists.txt`、Avatar 两测试及验证脚本、`docs/avatar.md` 与 R2 worker blob 一致；父 `examples/`、`gallery/`、`tests/token_gallery_frame_tests.cpp` 与 `83d26e7a` 一致，091 修复完整保留。父另补中英文 README、组件进度和本记录。

## main 实际命令

正式 VS MSVC x64、`windows-msvc-headless`、Ninja Multi-Config、BUNDLED，最多 2 jobs。通过 VSDevShell 后执行：

```text
cmake --preset windows-msvc-headless
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_avatar_theme_tests rynui_avatar_component_tests rynui_portable_theme_algorithm rynui_portable_theme_runtime rynui_popover_integration_tests rynui_timeline_review_tests rynui_notice_review_tests
ctest --preset windows-msvc-headless-debug --parallel 2 --output-on-failure -R "^rynui\.(avatar_.*|portable\.theme_(algorithm|runtime)|popover_(wheel|children|covered|clips)|timeline_review_(fill|document_hit)|notice_review_clip_contract)$"
out/build/windows-msvc-headless/tests/Debug/rynui_avatar_theme_tests.exe out/avatar-main-goldens
cmake --build --preset windows-msvc-headless-release --parallel 2 --target rynui_avatar_theme_tests rynui_avatar_component_tests rynui_portable_theme_algorithm rynui_portable_theme_runtime rynui_popover_integration_tests rynui_timeline_review_tests rynui_notice_review_tests
ctest --preset windows-msvc-headless-release --parallel 2 --output-on-failure -R "^rynui\.(avatar_.*|portable\.theme_(algorithm|runtime)|popover_(wheel|children|covered|clips)|timeline_review_(fill|document_hit)|notice_review_clip_contract)$"
python tests/verify_avatar_theme_baseline.py out/avatar-main-goldens
```

全部 exit 0。配置 7.1489312 秒；Debug 实际编译/链接 165.614216 秒，18/18 tests、CTest real 2.86 秒；Release 实际编译/链接 172.190488 秒，18/18 tests、CTest real 1.28 秒。包含 Avatar 九项、两项共享 Theme、Popover 四项及 notice/Timeline 裁剪与命中回归。保留既有 warning 和 VSDevShell 的 `vswhere.exe` 提示。日志为 `out/088-main-{debug,release}-{build,test}.log`、对应 CTest XML、`out/088-main-debug-{configure,goldens}.log`；忽略目录的 `out/088-main-msvc.ps1` 只设置环境并记录输出、计时和退出码。

## 保留性与范围

五份 main fresh goldens 与已审 snapshots 逐字节一致，深比较保留五份 JSON 共 235 个旧顶层字段及其全部嵌套类型/值、36 个旧 IIFE（34 直接 + 2 嵌套）原文/顺序、196 个旧 identity ordinal，仅追加 Avatar。四个 main 新建 Avatar Theme/component Debug/Release PE 的 stack reserve 均为 1,048,576 bytes。clang-format 22.1.3 检查 731 files、0 failures；显式 OpenSpec 1.14.0 doctor healthy、strict 89/89、diff check 通过。没有升级全局工具或扩大栈。

本记录不代表 Avatar 交互 Gallery、完整 Gallery/native startup、组件 GPU/人工视觉、Linux、系统 IME 或 AT 验收。目录和样例留给本批统一更新；图片管线、Group 任意富 slot、responsive 等首版排除见 `docs/avatar.md`。父只推 main，不推 t3 分支，没有 PR 或 archive。
