# 088 验收证据

日期：2026-10-26。实施范围仅 app 分配的隔离 worktree；基线 `8056de67953ee73015c7559f1782033d85e08573`。不包含父线程 main 集成或 Astra 审查结果。

## 锁定源码与范围

- `git -C D:/code/RynUI/out/upstream-ant-design-6.6.5 rev-parse HEAD` 实测 `4a39f54842eade4e565ab336ef6097cd7e723cdd`。
- 读取 avatar 的 index.en-US.md、Avatar.tsx、AvatarGroup.tsx、style/index.ts、demo snapshot，以及 theme alias/font/compact 派生源码。modern API 使用 medium 与 max.count；前 count 个原始头像后追加 +N，默认 hover/top。
- 无公开 Image pipeline。本次交付 text/icon Avatar；不暴露 src/srcSet、图片联网/文件解码、CORS、responsive breakpoint 或 Group 任意富 slot 的 stub。
- 独立 Avatar 的 typed icon/content 只挂载一次；Group 的固定 key 文字/原生 IconName 各保留 inline/overflow 展示节点。circle 使用内接矩形安全裁剪，区别于 Web 弧形 mask；详见 docs/avatar.md。

## 平台通用验证（实际 Windows MSVC headless）

使用 VS 2026 MSVC x64、CMakePresets 的 `windows-msvc-headless`、Ninja Multi-Config，未改变默认 PE stack 或 renderer ABI。先加载 VS Developer Shell，再执行：

```text
cmake --preset windows-msvc-headless
cmake --build --preset windows-msvc-headless-debug --target rynui_avatar_theme_tests rynui_avatar_component_tests --parallel 2
ctest --preset windows-msvc-headless-debug -R '^rynui\.avatar_' --output-on-failure
out/build/windows-msvc-headless/tests/Debug/rynui_avatar_theme_tests.exe out/avatar-goldens
C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe tests/verify_avatar_theme_baseline.py out/avatar-goldens
```

- Debug 定向 CTest **8/8** 通过；最终 component 源码运行耗时 1.91 秒。测试：theme、text、group、clips、retention、gates、lifecycle、validation。golden writer 随后改为 binary 原样写 diagnostic JSON，单独重建 Theme target 并重新运行通过。
- five fresh goldens 的所有旧 typed 字段深比较通过：**34 个旧 IIFE 原文/顺序、完整旧组件派生段、196 个旧 identities、5 份 JSON 中 235 个旧顶层字段及其嵌套值**均保持；仅新增 avatar 与 identity 值变化。已用 fresh 结果更新五份仓库 golden。
- 真实字体测量验证等长 `WWWW` 与 `iiii` 得到不同自然宽度与字号；实际 R8 atlas coverage、marker quad、hit clip、circle 背景几何与 Theme material 断言通过。
- 验证 Group overlap、count=0、实际隐藏 Avatar Popover、动态显隐与固定节点身份、有限宽/短高/零恢复、pending hover 取消。富 Input 的编辑器/草稿/ref 保留，controlled Select 普通关闭拒绝与隐藏边界强制关闭，Spin/Modal gate，document hit clip，同步 blur 卸载与同 ref 新 generation 重绑均通过。
- 清理断言覆盖 nodes、text scenes/glyph instances、surfaces、interactions、owned blocks 与 idle deadline。未运行历史 full suite 或 Gallery。

本机日志（ignored，未作为发布资产提交）：`out/build/avatar-configure.log`、`out/build/avatar-debug.log`、`out/build/avatar-debug-golden-build.log`、`out/build/avatar-debug-ctest.log`；fresh JSON 位于 `out/avatar-goldens/`。

## 收口状态

Release 定向构建与最终收口进行中，不以 Debug 结果代替 Release。当前证据不代表 GPU、原生窗口、Linux、系统字体、系统 IME、AT 或 main 已验收。父线程负责 Astra 审查与集成发布；本 worker 不执行 Git 远端操作、push、PR 或 archive。
