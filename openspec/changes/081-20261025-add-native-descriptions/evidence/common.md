# Descriptions 平台通用验收

## 环境与范围

2026-10-25，Windows、MSVC 19.51.36256、Ninja Multi-Config，官方 `windows-msvc-headless` preset，build parallel 4。基线先为 `62c990a1b494532c358ac487b2bb5931c08a7231`，本地阶段 `2bdf090` 干净后显式 merge `c083fed5a72f644b491118c6440908e0354d241e`，merge commit `f8191ce`。未 fetch/rebase/push/创建 PR/archive/子代理；独立 Astra review 由父代理负责，此证据不代替审查结论。

## 精确执行入口

在仓库根 PowerShell 执行（helper 内含完整目标列表与 anchored CTest filter）：

```powershell
& ./openspec/changes/081-20261025-add-native-descriptions/evidence/run-common.ps1 -Configuration Debug
& ./openspec/changes/081-20261025-add-native-descriptions/evidence/run-common.ps1 -Configuration Release
```

[run-common.ps1](run-common.ps1) 依次执行官方 configure、`cmake --build --preset windows-msvc-headless-<config> --parallel 4 --target ...`、当前构建 Theme algorithm 的 `--write-goldens`、双基线深比较、Debug 文档示例编译和 33 项 CTest。无需仓库依赖安装；VS/Python 路径采用本机已授权工具路径。

## 已完成证据

- Debug：**33/33，26.02 秒**，日志 `out/descriptions-final-debug.log`；Release：**33/33，13.39 秒**，日志 `out/descriptions-final-release.log`。两者均实际重建本树的目标并运行。
- 其中 Descriptions 自身 **8 项**：public API、packing model、retained component、Theme，以及 popup/cleanup/boundary/scene 四个专项。其余 25 项限于合并后受影响的 Breadcrumb/Steps、Date/Form、Feedback/Collapse、Theme、Button/Input/Select、Modal、focus、Prop 与 backend/Core 守卫；没有全套平台矩阵。
- 实际 separator quad 和标签/值/行坐标；Default/Middle/Small padding 影响实际高度；title/extra、三列 span 截断/末行补齐、显隐重排、非法 reactive column 原子拒绝与恢复。
- 富 Button 事件、Select 实际选项/值变化；无焦点 default-open popup 隐藏后关闭且普通更新不复活；关闭回调中销毁父组件。
- Input 编辑草稿、旧输入会话退休、实际文本 viewport 捕获、blur 回调销毁；同窗口 4 轮 hidden/live teardown，每轮 owned block collection 回到基线并保留其他 owner 的 block。
- Modal/Spin 输入边界、popup 清理与恢复，普通 Theme 变化保留 editor 和 session；富子控件的 outward keyboard focus coverage 实际大于自身 bounds，未被紧贴 slot root 裁掉。
- Default/Dark/Compact × 500×220、140×55、0×0 与恢复的实际 glyph、quad、hit clip；颜色确实到达 glyph material；idle 无 frame/deadline 或重复 geometry/material，Prop 变化走正常唤醒。
- 两段 docs/descriptions.md C++ 示例由 [extract-doc-examples.py](extract-doc-examples.py) 提取，`cl /nologo /std:c++20 /EHsc /utf-8 /c /Iinclude out/descriptions-doc-examples.cpp /Foout/descriptions-doc-examples.obj` 退出 0。
- clang-format **22.1.3** 检查 **644** 个自有源码，零失败；`openspec doctor --json` healthy；`openspec validate --all --strict --no-interactive` **81/81**；worktree/cached `git diff --check` 通过。081 apply 进度 **4/4**，不 archive。

## Theme 旧数据保护

[verify_preservation.py](verify_preservation.py) 对当前可执行文件生成的 default/dark/compact/dark-compact/compact-dark 五份 golden 做递归深比较，包括字段集合、数组顺序、数值和 JSON 类型：

- 对 `62c990a`：default/dark/compact 各 **2806**、两组组合各 **2807**，共 **14032** 个旧 typed values 相同；只允许新增 breadcrumb/steps/descriptions 和改变 global identity；旧 **28** 段 resolver IIFE 原文/顺序相同。
- 对 `c083fed`：default/dark/compact 各 **2926**、两组组合各 **2927**，共 **14632** 个旧 typed values 相同；只允许新增 descriptions 和改变 global identity；旧 **30** 段 IIFE 原文/顺序相同，最终 31 段。
- 本组件 colors/metrics identities 附加在现有导航 identities 之后。未改 generic descendant preflight、heap validation、config self guard、Prop binding bridge 或 create_default 静态配置。
- 本次补正 Dark labelBg 为锁定 colorFillAlter 的 0.04（默认 0.02），只改变新增 Descriptions 字段。最终字体/metrics 验证在 snapshot 发布前执行，含至少 0.5 logical dp、lround/uint32 可表示性、非负有限和组合溢出；覆盖 inactive seed、继承、独立算法、分组 dirty、后代拒绝及恢复。
- Debug 与 Release 分别由各自当前可执行文件生成五份金样，SHA256 比较 **5/5 字节一致**（Debug hash 留在 `out/descriptions-debug-golden-hashes.json`），最终再次执行双基线脚本通过。

## 交付边界

实现只发布 logical CPU scene v3/stores，不改变 renderer GPU ABI。Nav、Date/Form、Gallery 的实现与既有专项测试、README 与 `c083fed` 原样保持。未导入 TimePicker WIP、未占用 082。

这是 MSVC-headless 平台通用验收，不代表 GPU、Linux、系统 IME、AT、真实窗口人工视觉或 Descriptions Gallery 已完成。固定拓扑、等分列、无自动响应断点/locale/RTL/Web DOM/CSS 的边界见 [组件文档](../../../../docs/descriptions.md)。
