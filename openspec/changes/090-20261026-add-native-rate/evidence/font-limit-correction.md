# 父合并前字号上限补正

本问题由父线程在合并前检查提出，尚未进行首次 Astra 独立审查。本 worker 只补正 Rate，完成后停待父 fresh R1 full review。

## Before：实际复现

原干净 HEAD：`0190d4b12b441132ea608e5faeefe81e4f7a7c10`，分支 `t3/add-native-rate`，原隔离 worktree `C:/Users/jiang/.t3/worktrees/RynUI/t3-c74e4a3d`。先添加专项测试 harness，未修改生产实现时，以现有官方 `windows-msvc-headless` preset 新编译/链接该测试，最多 2 jobs：

```text
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_rate_font_limit_tests
out/build/windows-msvc-headless/tests/Debug/rynui_rate_font_limit_tests.exe --reproduce-before
```

实际使用 `Rate(RateProps{}.count(1).character(RateCharacter{u8"a"}))`，初始正常布局后，对实际 ThemeScope 更新 `rate.tokens.star_size=dp(100000000)`。日志：

```text
BEFORE: published star_size=100000000; FontRuntime invalid_pixel_size; notifications=0; atlas pages unchanged=1
```

Theme snapshot 和 generation 已改变，随后生产 Text 字体解析失败。观察订阅在 Rate 的失败通知之后，因此其计数为 0，不能用该计数掩盖 snapshot 已发布的事实。FontRuntime 明确返回 `pixel_size_configuration / invalid_pixel_size`；atlas 保持 1 页，无巨型 atlas 分配。

## After：最小生产补正

只修改 `src/theme/rate_theme.cpp` 的最终 Rate 校验：对 Small/Middle/Large 每个尺寸，分别使用 normal 的 `float(size * 1.0F)` 和 hover 的 `float(size * hover_scale)`；再以与生产 Text `lround` 相同的正数舍入语义计算像素。使用 double 的 `round` 结果比较边界，避免先执行可能溢出的整数转换，也避免把 `INT_MAX / 64` 转为 float 后扩大上限。MSVC 实际上限为 33554431 logical pixels。

专项覆盖精确乘积 `1082401 × 31 = 33554431`，但实际 float 乘积为 33554432 的反例；三尺寸均须拒绝。边界相邻合法 float 和 .5 logical dp（量化为 1）仍接受。正常尺寸和 hover 最大字号均在 resolve/preflight 阶段拒绝，保留旧 snapshot/generation/通知；祖先本身合法但与后代 hover_scale 组合非法时，整个继承树拒绝发布。合法恢复后，实际字符 `a` 的 normal 和 hover glyph 均可渲染。

未修改 FontRuntime、Text 生产量化、共享 window clip、renderer ABI、旧组件或其他 change。大 ThemeConfig/Snapshot 测试对象保留在 heap。

## 定向验证

在相同隔离树，以 MSVC 19.51、Ninja Multi-Config、官方 `windows-msvc-headless` preset、最多 2 jobs 新编译/链接补正后的生产 Rate 与专项可执行文件：

```text
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_rate_font_limit_tests
ctest --preset windows-msvc-headless-debug -R '^rynui.rate_font_limit$' --output-on-failure
cmake --build --preset windows-msvc-headless-release --parallel 2 --target rynui_rate_font_limit_tests rynui_theme_algorithm_tests
ctest --preset windows-msvc-headless-release -R '^rynui.rate_font_limit$' --output-on-failure
```

- Debug：1/1，通过；测试 0.39 秒，总计 0.41 秒。
- Release：1/1，通过；测试 0.15 秒，总计 0.17 秒。
- 覆盖直接三尺寸 normal/hover、float 乘积舍入、独立 seed、祖先/后代组合 preflight、snapshot/generation/通知的原子性，以及实际字体失败路径的合法恢复。合法大边界只运行 Theme resolution；实际字体/atlas 只使用正常字号，超限复现由 FontRuntime 提前拒绝。
- 之前通过的 9 项交互/生命周期/共享裁剪矩阵未重复。

Release 的 `rynui_theme_algorithm_tests.exe --write-goldens` 只生成五份 fresh 金样，不运行旧测试矩阵。随后 `git diff --exit-code 0190d4b12b441132ea608e5faeefe81e4f7a7c10 -- design-tokens/ant-design/6.6.5/golden src/component/window_component_services.cpp src/font/font_runtime.cpp` 通过：五金样、共享 window clip 和 FontRuntime 均未变化。既有 `verify-theme-compat.py` 也确认 16587 个旧 typed 叶字段、36 个旧 IIFE 和 196 个旧 identities 不变。

收口检查：`python scripts/format-code.py --check --clang-format 'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe'`；父给定 explicit OpenSpec 1.14 入口执行 `doctor --json` 和 `validate --all --strict --no-interactive`；`git diff --check` / `git diff --cached --check`。不访问远端，不 push/PR/archive/children，不改 ROOT/out 或 Gallery。

实际结果：clang-format 22.1.3 检查 738 个自有源文件，0 failures；OpenSpec doctor 为 healthy=true，strict 为 88 passed / 0 failed；两项 diff check 通过。本补正只有 Rate 最终校验、专项测试/CMake、公开文档和本 change 任务/证据变更。
