# 087 平台通用验收

## 环境与提交

- 日期：2026-10-25；实际平台 Windows x64，MSVC 19.51.36256.0，Ninja Multi-Config，`windows-msvc-headless`，构建最多 4 jobs。
- 隔离目录：`C:/Users/jiang/.t3/worktrees/RynUI/t3-471ecb3c`；分支 `t3/native-timeline`；父基线 `bfbb49234c945c52988b8474838d3ac88bbc84ec`。
- 规划：`e44ce8ba`；API/model/Theme：`edfaaffa`；retained 组件/测试/示例：`acf980cd`。仅本地提交，未 fetch/rebase/push/PR/archive，未新增 child。
- 本树 `out/upstream-ant-design-6.6.5` 的 `git rev-parse HEAD` 为 `4a39f54842eade4e565ab336ef6097cd7e723cdd`。只读复制获授权的锁定源码、依赖 `*-src` 和字体；CMakeCache、obj、lib、exe 均在本树新生成。

## 实际检查

Debug 最终去重组合 **25/25** 通过，11.75 秒：

- Timeline：model/public_api/theme/component/integration/region_clip/example，7 项。
- Descriptions：component/shared_theme；Steps：component/theme；Collapse：component/theme；feedback_component（含 Spin）。
- portable：default_theme/theme_public_api/theme_algorithm/theme_runtime/logical_scene_packing/switch_features/focus_lifecycle/reactive_source_lifecycle/select_theme/select_component/modal_boundary。

使用的 CTest 选择：

```text
ctest --preset windows-msvc-headless-debug -R '^rynui\.(timeline_.*|descriptions_(component|shared_theme)|steps_(component|theme)|collapse_(component|theme)|feedback_component|portable\.(theme_algorithm|theme_runtime|theme_public_api|default_theme|select_component|select_theme|modal_boundary|focus_lifecycle|reactive_source_lifecycle|logical_scene_packing|switch_features))$' --output-on-failure
```

Release 新构建的 Timeline model/public_api/theme/component/integration/region_clip/example **7/7** 通过，0.67 秒：`ctest --preset windows-msvc-headless-release -R '^rynui\.timeline_' --output-on-failure`。

clang-format 22.1.3 对 691 个自有源文件 `--check` 通过；OpenSpec 1.14.0 doctor 健康、strict validate 84/84 通过，`git diff --check` 通过。

## 真实场景覆盖

- Start/End/Alternate、源索引 placement override、reverse 实际位置/Tab、唯一 key 与非法 enum 拒绝；普通属性更新不重挂 Input/Button/Select，保留 editor、draft、refs 和 session。
- 空/全隐藏退出 Flex flow、不残留 gap；恢复 item 身份。
- 实际 retained dot outline/material、tail quad 与下一节点端点、反序最后一项无 tail；typed tone 与仅颜色 Theme 更新不触发 measure。
- 实际 R8 atlas texel center 经 glyph vertex 与 shader clip 采样，验证 title/content/icon 的可见 ink 不越分配区；极窄、短/零高、恢复与 Default/Dark/Compact 下的实际 surface/hit clip。
- 富 Title/Button、Content/Input、Icon/Select 使用自然尺寸；外扩 focus 有实际 coverage，外部 hardclip 正确截断。
- controlled close 拒绝保持 authority；隐藏强制 suppression 后普通更新不复活旧 popup，新 authority epoch 可重新打开。
- 隐藏与祖先 Modal/Spin 清理焦点、preedit/session、capture、popup；同步 blur/close 卸载与 blur 重绑使用完整 generation，旧 session 拒绝；owned blocks teardown 不删除 foreign owner。
- 静态 loading 和 idle 不持续请求帧、不重复发布 geometry/material；可编译示例在真实 host 挂载并更新。

## Theme 兼容性

新构建 `rynui_portable_theme_algorithm.exe --write-goldens` 生成 default/dark/compact/dark-compact/compact-dark 五份 JSON。以 `git show bfbb492:<path>` 读取旧值，递归逐字段比较类型与值：所有旧 typed 字段完全一致，仅增加 `timeline` 和改变 `identity`。

逐块提取 `theme_snapshot.cpp` 的 IIFE：旧 33 块与新文件前 33 块原文/顺序一致（只归一化换行），新文件共 34 块。旧 TokenIdentity 名称序列完全保留，只在 count 前追加 timeline_colors/timeline_metrics；Time 静态 default config、heap descendant preflight/self guard/Binding bridge 和 switch publish_theme helper 未修改。未扩大 PE 栈。

独立 Timeline Theme 测试验证继承、inherit=false、全局/组件算法、inactive seed、hash/equality/diagnostic、precise dirty、最终字体阈值/非法 metric、后代组合原子拒绝与恢复。

## 边界

本 change 仅实现 vertical、固定 keyed topology、静态 loading。horizontal、titleSpan、RTL、动画、Web AT、动态增删/任意排序和任意 style 明确排除；文本保持自然尺寸并裁剪，不自动换行。不改全局 README、component-completion、support-overlay 或 Gallery。没有 GPU/native-window/Linux/系统 IME/AT 验收证据；headless 输入/preedit 是真实 Core fixture 事件，不代表系统服务验收。未重复历史全套或长 Gallery，父负责独立评审与发布。
