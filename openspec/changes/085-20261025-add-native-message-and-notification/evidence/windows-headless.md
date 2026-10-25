# Windows headless 验收

日期：2026-10-25。平台通用逻辑在本 Windows 机器完成一次 Debug/Release 验收。

## 源码与工具链

- 隔离 worktree：`C:/Users/jiang/.t3/worktrees/RynUI/t3-f0b94dd3`，分支 `t3/native-message-notification`。
- 基点：`bfbb49234c945c52988b8474838d3ac88bbc84ec`；规划提交 `704f284d`；实现提交 `4f1264e94acc015c37a091642c0abfedf0e7cce1`。
- MSVC 19.51.36256.0、x64；官方 `windows-msvc-headless` / `windows-msvc-headless-debug` / `windows-msvc-headless-release` presets，Ninja Multi-Config，每次 build 最多 4 jobs。
- 只从 ROOT 的 `_deps` 复制 FT/HB/SheenBidi/utf8proc 的 `*-src` 与锁定验证字体；本 worktree 独立 configure/build。没有复制或运行 ROOT 的 CMakeCache、Ninja tree、exe 或 obj，也没有修改 ROOT。
- Ant Design 只读来源为 `D:/code/RynUI/out/upstream-ant-design-6.6.5`，实际核对 HEAD `4a39f54842eade4e565ab336ef6097cd7e723cdd`；未在线查询其他版本。
- 初始 PATH 的旧 pnpm shim 指向 OpenSpec 1.13.1。最终定位并直接执行 pnpm 已安装的 **1.14.0** 包入口；重新读取 proposal/design/specs/tasks 指令，核对 context/status/apply instructions，并使用 1.14.0 重跑 doctor 与 all strict。未修改全局安装或 PATH。

## 构建与测试

Debug、Release 均成功构建以下目标及其正式依赖；没有运行历史全套或 Gallery：

```text
rynui_notice_model_tests
rynui_notice_theme_tests
rynui_notice_component_tests
rynui_portable_theme_algorithm
rynui_portable_theme_runtime
rynui_portable_theme_scope_component
rynui_portable_default_theme
rynui_portable_switch_features
rynui_portable_window_overlay
rynui_portable_select_component
rynui_portable_modal_boundary
rynui_portable_input_component
rynui_portable_prop
rynui_portable_rounded_effect_scene
rynui_portable_logical_scene_packing
```

两次正式 CTest 使用同一去重集合：

```text
ctest --preset windows-msvc-headless-debug -R '^rynui\.(notice_.*|portable\.(theme_algorithm|theme_runtime|theme_scope_component|default_theme|switch_features|window_overlay|select_component|modal_boundary|input_component|prop|rounded_effect_scene|logical_scene_packing|backend_boundaries))$' --output-on-failure
ctest --preset windows-msvc-headless-release -R '^rynui\.(notice_.*|portable\.(theme_algorithm|theme_runtime|theme_scope_component|default_theme|switch_features|window_overlay|select_component|modal_boundary|input_component|prop|rounded_effect_scene|logical_scene_packing|backend_boundaries))$' --output-on-failure
```

- Debug：**24/24 passed**，15.57 秒。
- Release：**24/24 passed**，7.61 秒。
- 新增 11 项：notice_model、notice_theme、notice_event、notice_retention、notice_clock、notice_lifecycle、notice_boundaries、notice_popup、notice_clipping、notice_example、notice_docs_example。
- 实际验证按钮事件创建、五种状态、真实关闭/action Button、字符串与富内容、Input 草稿/ref/session 保留、单区域替换、maxCount 淘汰、默认/持久/正 duration、失活/hover 继续到期、倒退时钟夹持和 idle 无 deadline。
- 生命周期验证到期回调重开同 key、回调延长另一到期 entry、getter 同步卸载/重绑、slot 同步卸载、slot 抛异常保留旧内容、focused 关闭、onClose 卸载父 scope。
- 输入组合验证 Modal、Spin、隐藏祖先、foreign blocks、子 Select popup 的 wheel/Escape、covered sibling 命中、通知背景最终 wheel 回退，以及 180×90、20×10、1×1 视口与四角堆叠裁剪。
- API 文档片段与 `tests/support/notice_example.hpp` 去空白逐字符比较；同一 fixture 编译并实际派发按钮/action 事件。
- 配置与 `portable.backend_boundaries` 通过；未引入 renderer/SDL/OS 依赖到 Core，未改 logical scene v3、GPU ABI、上传路径或 shader。

## Theme 与格式

- 新构建的 Debug `rynui_portable_theme_algorithm.exe --write-goldens` 生成 default、dark、compact、dark-compact、compact-dark 五份 JSON。
- 与基点 JSON 递归深比较：所有旧 typed 字段和值完全相同；仅新增 message/notification 字段与变化的 identity。
- 对基点与当前 `theme_snapshot.cpp` 的全部 IIFE（含 empty/result 赋值式 IIFE）逐段比较：父已有 **33 段原文与顺序完全相同**，只追加 message/notification 两段。TokenIdentity 只在 descriptions_metrics 后追加 8 项。
- Theme 测试覆盖独立 override、inherit/reset、全局与组件 seed algorithm、inactive seed、颜色/metric/order identity、非法字体/auto/非有限值、descendant preflight 原子失败与恢复。
- 保留 Time 静态默认配置、heap descendant preflight、self guard、Binding bridge 与 switch_features 的 publish_theme helper；对应文件/原逻辑未改，受影响回归通过。
- `dumpbin /headers` 核对新 Debug notice component 测试程序：stack reserve `100000`（十六进制），即默认 **1 MiB**；未扩大 PE 栈。
- `python scripts/format-code.py --check` 使用 clang-format **22.1.3**：688 owned source files、0 failures。本机 Python 使用实际 `Python310/python.exe` 入口。
- OpenSpec **1.14.0** doctor healthy；`validate --all --strict --no-interactive` **84 passed / 0 failed**；`git diff --check` 通过。

## 边界

loading 首版是静态图标，无动画开关；不提供 hover/失活暂停、进度条、全局 singleton/service、promise 或额外通知滚动容器。挂载中的 slot 仍遵守 Runtime 不重入 mount 的合同。

这些结果不证明 GPU、真实 native 窗口、Linux、系统 IME 或 AT 行为。README、全局完成表、support overlay 和 Gallery 交父集成；本分支没有 children、PR、archive、Git 远端访问或 push。
