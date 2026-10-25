# 084 平台通用验收

日期：2026-10-25。工作目录：`C:/Users/jiang/.t3/worktrees/RynUI/t3-a626df41`。分支：`t3/native-popover`。起点：`bfbb49234c945c52988b8474838d3ac88bbc84ec`，规划提交：`19d94466`。

## 构建和范围

- 本树全新 `windows-msvc-headless` configure，Ninja Multi-Config，MSVC 19.51.36256.0；`windows-msvc-headless-debug` / `windows-msvc-headless-release` 针对性构建通过，每次最多 `--parallel 4`。
- 本地锁定 Ant Design 6.6.5 源码只读核对，HEAD `4a39f54842eade4e565ab336ef6097cd7e723cdd`，涉及 Popover index/PurePanel/style。
- FreeType/HarfBuzz/utf8proc/SheenBidi、字体和 Unicode 数据由本地缓存只读复制到本树 out；在本树重新 configure/build，未复制旧 build tree、CMakeCache、exe 或 obj。
- 全部证据限于平台通用 model/Theme/retained scene/input 合同。未运行原生窗口、GPU、Linux、system IME 或 AT；没有 Gallery 验收，不据此声明这些平台能力通过。

## 实际测试

以下同一去重组合 Debug **31/31**（20.58秒），Release **31/31**（12.22秒）通过：

- 新增14项：`popover_public_api`、`popover_model`、`popover_theme`、`popover_examples`、`popover_retained`、`popover_triggers`、`popover_boundaries`、`popover_lifecycle`、`popover_validation`、`popover_gates`、`popover_wheel`、`popover_children`、`popover_covered`、`popover_clips`。
- 17项相关回归：`time_navigation_merge`、`time_picker_integration`、`date_picker_component`、Date review 的 wheel/metrics/inheritance/preflight/form_reset，以及 portable 的 dropdown_component/default_theme/theme_scope_component/switch_features/theme_algorithm/theme_runtime/tooltip_component/popconfirm_component/backend_boundaries。

```text
ctest --test-dir out/build/windows-msvc-headless -C Debug -R "^rynui\.(popover_.*|date_picker_component|date_picker_review_.*|time_picker_integration|time_navigation_merge|portable\.(theme_algorithm|theme_runtime|theme_scope_component|default_theme|switch_features|tooltip_component|popconfirm_component|dropdown_component|backend_boundaries))$" --output-on-failure
ctest --test-dir out/build/windows-msvc-headless -C Release -R "^rynui\.(popover_.*|date_picker_component|date_picker_review_.*|time_picker_integration|time_navigation_merge|portable\.(theme_algorithm|theme_runtime|theme_scope_component|default_theme|switch_features|tooltip_component|popconfirm_component|dropdown_component|backend_boundaries))$" --output-on-failure
```

组件测试包含真实 Input session/draft/ref 与 Button pointer/Enter/Space，普通属性/Theme/开闭不重跑 slots；无焦点受控 open 拒绝普通关闭后仍被 Modal/Spin/隐藏/失活强制 suppression，子 popup 不复活，销毁不清除 foreign block。生命周期覆盖 onOpenChange/onClick/onBlur、激活 getter、open authority getter 同步卸载及控件 generation 重绑。

组合测试在一次同步内检查 Select、Date、Time 和 Table 的最近 popup wheel 归属、内容先处理和边界最后 fallback、覆盖 sibling 不穿透、子 Escape 优先。Date footer 中的 Table 增加与 Time 相同的定向同帧几何刷新；父 Popover 内容中的 Table 也在位置提交后定向刷新。组合 hover/focus/click 使用按压开始时的开闭意图，避免 focus 打开后 click 抬起立即关闭。

裁剪检查使用实际 retained glyph instances、mark quads、hit snapshot，以及 focus outline 的真实 coverage 和 ancestor clip，覆盖极窄视口、未调整越界、恢复、外扩焦点与外部硬裁剪。宿主要求正数视口，测试最小为1×1。

## Theme 与固定合同

新构建的 `tests/portable/Debug/rynui_portable_theme_algorithm.exe --write-goldens` 生成全部五份 typed golden；[compare-theme.py](compare-theme.py) 对指定父提交深比较：五份共 **15,372** 个旧叶子值保持，只新增 `popover` 与更新 snapshot identity；旧 **33个 IIFE 原文及顺序完全保持**，TokenIdentity 仅末尾追加。

[check-stack.py](check-stack.py) 读取 Debug/Release 六个新测试程序的实际 PE32+ header，12个程序均为 **1,048,576 bytes** stack reserve。保留 Time 静态 default config、Theme heap descendant preflight/self guard、Binding bridge 和 Switch publish_theme helper；没有扩大栈。

clang-format **22.1.3** 全仓自有源检查689个文件，0 failures；`openspec doctor --json` 健康，`openspec validate --all --strict --no-interactive` **84/84**，`git diff --check` 通过。

本任务只做本地英文 Conventional 提交，没有远端 Git 操作或 push，没有 children/PR/archive，也未修改全局 README、component-completion、support-overlay 或 Gallery。
