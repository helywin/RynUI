# Astra R1 分区裁剪修复

## 输入与独立阶段边界

父代理提供的独立 Astra R1：1 项 P2，无 P1，其余首版实现实质齐备。起点为干净 `1e27a71fca758d9b3891c3d3152aa0b87bcba64b`，先取消 tasks 2.1/3.1 勾选；本阶段尚未 merge 新 main。R1 修复仍需父代理安排全新独立审查，本证据不声称 CLEAN。

## 修复前真实失败

仅加入回归、未修改生产代码时，MSVC Debug 构建 `rynui_descriptions_region_clip_tests`，运行：

```powershell
cmake --preset windows-msvc-headless
cmake --build --preset windows-msvc-headless-debug --parallel 4 --target rynui_descriptions_region_clip_tests
ctest --preset windows-msvc-headless-debug -R '^rynui.descriptions_region_clip_' --output-on-failure
```

`out/descriptions-r1-before.log`：**0/4，CTest exit 8**。测量真实 label/value node allocation；从每个 retained glyph 的 UV、vertex、clip_bounds 和实际 atlas R8 texel center 计算 shader clip 后可见墨迹，不以 glyph 矩形交集代替真实覆盖率。

| 路径 | label/value 宽度 | 区外原始墨迹采样 | shader clip 后区外仍可见 |
| --- | --- | --- | --- |
| column(1), colon(false), width20，横向无框 | 10 / 10 | 23 | 23 |
| 横向 bordered（padding=1） | 9 / 9 | 29 | 29 |
| 纵向无框，首列有限分区 | 4 / 4 | 57 | 57 |
| 纵向 bordered（padding=1） | 9 / 9 | 29 | 6 |

初始标签和值均为普通 `汉`，不缩字号，不改变 TextEngine 对超宽单字的保留行为。后续回归还覆盖带 word joiner 的 `W\u2060W`、短/零与恢复。

## 最小修复

- Descriptions 被动 slot 设置 content/hit clip。layout、测量、span packing、旧标签位置和子控件几何算法不变。
- Node 增加默认为 true 的 `clip_decorations`，内部 `ClipPurpose` 查询保留原 clip chain；仅 Descriptions 的被动 slot 设置 false。普通节点两种查询仍完全相同。
- Button 的外扩 focus/shadow、Input 的 outer shadow/focus 查询 decoration clip；文字、selection、fill/border、hit 仍查询 content clip。Input presentation cache 同时记录两种 clip，普通 Theme 更新不 reset draft。
- Modal 的 retained surface fill 保持内容 clip，外扩 decoration 通过同一祖先链的 decoration 查询；item、父 clip boundary 和窗口 clip 保留。无新布局框架或 renderer/GPU ABI 改动。

## 修复后实际证据

新增回归 Debug：**5/5，0.51 秒**，`out/descriptions-r1-after-new-debug.log`（verbose）。上述四条路径的 shader clip 后区外采样全部为 **0**，原始超宽字形仍保留；额外检查实际 Text mark quads。

第五项在真实 Modal 内容中组合 retained label、Flex、Button、Input：实际 glyph/atlas、mark quads、Modal clipped-fill coverage、hit record 与分配区一致；应用真实 ancestor hard clip 后，Button/Input 的外扩 focus coverage 仍大于零，事件、输入会话和文本交付正常。普通 Dark Theme 更新保持 draft/session 和控件 bounds；横纵/有无框、短/零/恢复及隐藏恢复保持 editor/slot 身份。焦点测试检查实际 focused id；request_focus 的 false 也可能表示焦点本已相同。

## 定向旧目标与 Release

[run-r1.ps1](run-r1.ps1) 记录完整目标列表、官方 presets、`--parallel 4`、fresh goldens 和 filter。Debug 的新增 5 项单独完成后，旧目标使用去除 region_clip 的 filter 运行一次；Release 使用默认完整去重集合。没有重跑无关 33 项矩阵。

实际命令：

```powershell
ctest --preset windows-msvc-headless-debug -R '^rynui.descriptions_region_clip_' --verbose
& ./openspec/changes/081-20261025-add-native-descriptions/evidence/run-r1.ps1 -Configuration Debug -TestFilter '^rynui\.(descriptions_(model|public_api|component|theme|integration_(popup|cleanup|boundary|scene))|feedback_component|portable\.(input_component|button_component|button_scene_service|focus_(state|lifecycle)|select_component|theme_runtime|modal_(boundary|component)))$'
& ./openspec/changes/081-20261025-add-native-descriptions/evidence/run-r1.ps1 -Configuration Release
```

结果：Debug 新增 **5/5，0.51 秒**；Debug 去重旧目标 **18/18，13.50 秒**（`out/descriptions-r1-debug.log`）；Release 去重完整集合 **23/23，6.92 秒**（`out/descriptions-r1-release.log`）。所有目标均为本树正式 MSVC/Ninja Multi-Config 重新构建，最大 4 jobs。

两配置分别由当前 Theme algorithm 可执行文件生成五组 golden，`git diff --exit-code 1e27a71fca758d9b3891c3d3152aa0b87bcba64b -- design-tokens/ant-design/6.6.5/golden` 均为零差异。深比较继续保留原 `62c990a` 的 **14032** / `c083fed` 的 **14632** 个旧 typed leaves 与 **28/30** 段 IIFE，最终仍为 31 段。Debug 文档两段 C++ 示例实际 MSVC 编译退出 0。

clang-format **22.1.3** 检查 **645** 个自有源码零失败；doctor healthy；strict **81/81**；worktree/cached diff checks 通过。恢复 tasks 2.1/3.1；新 main 合并任务保持未勾选，独立保存本修复阶段。

## 后续合并

本修复验证后先保存干净英文 Conventional Commit，再显式 merge 已发布 `580a8c3d235c22eb95b11688b509ee1e2ea65e44`。Time/导航/Gallery 及共享 Theme 的最终共存验收另行记录，保留该 main 的 31 个旧 IIFE 和 TokenIdentity 值/顺序，Descriptions 附加在 Time 后；不 blind rebase，不导入未发布 WIP。
