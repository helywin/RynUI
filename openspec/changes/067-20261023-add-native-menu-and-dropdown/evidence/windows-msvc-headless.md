# 平台通用验收记录

环境：Windows x64，MSVC 19.51.36256，Ninja Multi-Config，`windows-msvc-headless` preset，独立工作树 `t3/native-menu-dropdown`，build/test 并发上限 4。设计参考仍为锁定 Ant Design 6.6.5，CPU scene v3 与 packed GPU ABI v2 未改动。

## 聚焦验收

- `rynui.portable.menu_component`：受控选择/展开拒绝与回写、单选/多选、禁用/组/分隔线、Inline 方向键/首末/激活、滚动、稳定行与 slot、Default/Dark/Compact、纯颜色不测量、回调卸载与资源清理通过。
- `rynui.portable.dropdown_component`：Button Enter/Space/pointer 原 onClick 保留、click/context-menu 坐标、Up/Down、Escape/Tab、外部焦点、controlled refusal、翻转/钳制、popup clip、初始隐藏 Modal、环境 suppression、防复活、嵌套 Dropdown/Select 与回调卸载通过。另覆盖 disabled/loading Button 不通过 wrapper、pointer 或方向键绕过原生 activation gate。
- 同一测试还覆盖 Menu 位于滚动 Modal 中时的 wheel 交接、translation、背景 clip 与真实 hit/pointer 激活。
- `switch_features`、`theme_runtime`、`theme_algorithm` 与上述两个测试的聚焦 CTest 5/5 通过。
- Input allocation 短探针：256 inputs，selection 与 composition-selection 各 64 次更新；两种模式均为 `allocations=0`、`dispatch_allocations=0`、`sync_allocations=0`。

## 发现并修复的回归

首次 Debug 完整 CTest 为 115/117 通过：Switch 测试出现 Windows stack overflow (`0xC00000FD`)，Input 选择探针每次同步分配一次（20,000 次更新 / 20,000 次分配）。

- Dropdown 无实例时复制空 vector，MSVC Debug iterator proxy 产生分配；现在在复制前返回，保留关闭/卸载期间的回调安全遍历。
- 新 Token 使 Theme 值更大，既有提交函数按值传递 config 与 snapshot 加重嵌套更新的栈占用；内部 commit 改为 `const ThemeConfig&` / `ThemeSnapshot&&`，保持公开 API 和 immutable snapshot 合同。未提高线程栈大小、修改或跳过原测试。

## 完整检查

完整检查针对源码提交 `289c821e5001cca45dec00b81aff6737c48d619e`（后续仅记录证据）：

- `cmake --build --preset windows-msvc-headless-debug --parallel 4` 成功；`ctest --preset windows-msvc-headless-debug --parallel 4`：117/117 通过，266.42 秒。
- `cmake --build --preset windows-msvc-headless-release --parallel 4` 成功；`ctest --preset windows-msvc-headless-release --parallel 4`：117/117 通过，16.44 秒。
- 完整 allocation fixture 在两种模式下各执行 20,000 次更新并通过零分配断言；Debug 该测试耗时 266.41 秒。64 次短探针的原始计数见上文，未将短探针冒充完整执行。
- 最终 native trigger gate 的 Menu/Dropdown 聚焦 CTest 也在 Debug/Release 各 2/2 通过。

`format-code.py --check` 使用 clang-format 22.1.3 检查 506 个自有源文件，0 failures；OpenSpec doctor healthy、全部严格校验 62/62 与 `git diff --check` 通过。

## 交付边界

这是 Windows 上的平台通用 headless 合同证据，不是原生 GPU/视觉、Linux 或 IME 验收。Gallery、全局支持 overlay/count、README 和完成清单由父任务集成。不同 priority 的嵌套 popup 排序依赖 Popconfirm 共同 child-layer 修复（3ace95a 分支）；本变更没有复制 `ComponentHost` 的排序修改。

Horizontal overflow、vertical flyout、hover delay、split Button、popup renderer 与 Web/DOM/CSS API 不在首版内；Inline 子菜单、固定拓扑与响应式项状态的具体边界见 `docs/menu-dropdown.md`。

依最新用户策略，后续提交仅保留本地，父任务负责合并、最终验证和推送 main；无 PR 或 archive。

## Astra R1 回归修复（2026-10-23）

针对 `644a0ecbbecc426ebb69a354db07f4a0f7d1624b` 的三个 P2，先仅增加测试、保持实现不变，逐例执行 Debug binary：

| 独立 case | 修复前 exit | 实际失败断言 |
| --- | --- | --- |
| Dropdown `wheel-select` | 1 | `parent Menu stole overlapping Select wheel` |
| Dropdown `wheel-dropdown` | 1 | `parent Menu stole nested Dropdown wheel` |
| Dropdown `short-label-width` | 1 | `short label row did not fill popup inner width` |
| Menu `reactive-reveal` | 1 | `reconciled active row stayed outside Menu viewport` |

修复范围仅为两个 component 及其测试：Menu 根据最上层 hit 的 component owner 链处理 wheel，跨子 window layer 时留给其 host；Dropdown 在自然 popup 测量后以最终内宽/内高测量 Menu；Menu 活动行重新协调请求 reveal 时增加 Layout/HitTest invalidation。未修改共享 paint 排序、service dispatch 或 renderer。

回归验证直接检查重叠位置的真实 topmost hit、子层 scroll 增加且父层不变、子层滚动边界不泄漏给父 Menu、露出父菜单区域可滚动及 cleanup；短标签检查实际 Menu/行 node bounds 与右侧 pointer 命令；禁用末行检查新活动首行完整位于 viewport、Enter key 正确、行 identity 不变。既有受控拒绝、初始 Modal、嵌套 popup、Button onClick、clip/focus/cleanup 测试继续通过。

最终格式化源码使用同一 MSVC headless preset、每次最多 4 jobs：

```text
cmake --build --preset windows-msvc-headless-debug --parallel 4 --target rynui_portable_menu_component rynui_portable_dropdown_component
ctest --preset windows-msvc-headless-debug --parallel 4 -R "^rynui.portable.(menu_component|dropdown_component)$"
cmake --build --preset windows-msvc-headless-release --parallel 4 --target rynui_portable_menu_component rynui_portable_dropdown_component
ctest --preset windows-msvc-headless-release --parallel 4 -R "^rynui.portable.(menu_component|dropdown_component)$"
```

- Debug：2/2 通过，0.83 秒；Release：2/2 通过，0.49 秒。
- 四个独立 case 另在 Debug/Release 各执行一次，8/8 exit 0。可向上述测试 executable 传入表中 case 名单独复核；不带参数执行全部组件合同。
- `format-code.py --check`：clang-format 22.1.3，506 files，0 failures；doctor healthy；严格 validate 62/62；`git diff --check` 通过。
- 本地原始日志：`out/r1-reproductions.json`、`out/r1-fixed-cases.json`、`out/r1-debug.log`、`out/r1-release.log`、`out/r1-format.log`、`out/r1-doctor.json`、`out/r1-validate.log`。构建目录和日志保持 ignored，不作为仓库源码提交。

依评审指示未重复原已通过的完整 117 项套件；不增加原生 GPU/Linux/IME 声明。所有三个 finding 均已复现并修复，无跳过项。本轮只作本地提交，不 push、PR、archive 或改动 root/Gallery。

实现与测试的本地提交：`bedc46539fd65b751a84e48b5e39d2c527ed9a0c`（`fix: correct Menu popup scrolling and row layout`）；后续收口提交仅记录该 SHA 与任务完成状态。

## 独立 main 集成（2026-10-23）

工作树 `t3-de868eb7` / 分支 `t3/integrate-menu-input-number` 从 `69689046bc9a99b94c98086cb3c849b79015acbc` 合并本变更 `4fa26a9787c25a706ac1df39f687d856dd0c3851`。保留已有 Modal、Drawer、Popconfirm、Tag/Badge 与 Alert/Spin/Progress 的 Theme、service 和 CMake 接线；保留 main 的 ThemeScope 引用提交、reference_cell 默认主题栈占用修正与共同 popup child-layer 排序。未涉及父任务的 Table/Gallery/全局支持文档。

- 使用新构建的 Debug Theme executable 重新生成五份 golden；与 `6968904` 的 JSON 语义比较：除 `identity` 外所有旧字段完全一致，仅增加 `menu` / `dropdown`。
- `windows-msvc-headless` / MSVC 19.51.36256，Debug 七项聚焦 CTest 全部通过（6.02 秒）：Menu、Dropdown、Theme algorithm/runtime、Modal boundary/component、Popconfirm component。
- clang-format 22.1.3 检查 540 个自有源文件，0 failures；doctor healthy；strict validate 67/67；diff check 通过。
- 此阶段仅作本地合并提交；后续 InputNumber 合并后单独记录最终 Debug/Release 集成结果，不重复完整套件。
