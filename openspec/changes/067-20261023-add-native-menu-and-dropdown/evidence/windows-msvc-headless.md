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
