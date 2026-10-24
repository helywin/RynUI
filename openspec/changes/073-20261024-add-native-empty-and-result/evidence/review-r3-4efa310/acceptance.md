# Astra R3：字体范围与 Theme 发布原子性

2026-10-25；工作树 `C:\Users\jiang\.t3\worktrees\RynUI\t3-2b08410a`，分支 `t3/native-empty-result-components`。从干净的 2425a4c 本地 `git merge --ff-only 4efa310a256019192e01f0738ee226d16ad44396`，未操作 origin 或复制 root 未提交文件。实际运行选项为 `serviceTier=default`；没有启动 quota watcher、子任务、PR、push 或 archive。

## 修复前实际复现

先在原始 4efa310 Theme 源码上编译新增回归，再运行组件 executable。`before-fix.log` 保存实际退出码 1 的输出：47 个 case 通过、39 个失败；原有 margin 和图示零/极小尺寸回归仍通过。

日志中的 font 0/1/2 分别为 `Empty.font_size`、`Result.title_font_size`、`Result.font_size`。三个 `.49F` 响应式更新都实际报告 `rejected=0 published_changed=1 error=Input validation fonts failed to load`：字体 resolver 收到量化后的零像素请求，ThemeSnapshot identity 已变化才抛错。初始 `.49F` 配置也进入子 slot 后失败；空 description/title/subtitle 不避开固定挂载的字体更新。测试没有替换 resolver 来掩盖问题。

## 最小实现与合同

仅在 `apply_empty_override` / `apply_result_override` 现有长度解析后增加两处 guard，共 **6 行**。最终解析的 Empty font_size、Result title_font_size / font_size 必须为有限固定长度且 **>= 0.5 dp**；现有固定长度检查继续拒绝 auto、零、负数与非有限值。所有小于 0.5 的正数在发布前拒绝，不只特殊处理 `.49`。

该阈值来自已有 Text 的 `lround(logical_font_size)`：0.5 对应 1 个逻辑像素，`nextafter(.5F, 0)` 对应零。DPI 在此逻辑量化之后影响栅格尺寸，所以 2× DPI 不会使 `.49` 变得有效。保持此前正常字号、默认值、字体选择和量化规则；不修改共享 Text、FontRuntime 或 ThemeScope 发布代码，不捕获发布后的异常。既有内置 Icon 的零、`.01`、`.49` 尺寸仍有效，图示范围与文字字号范围独立。

## 回归和实际 scene 证据

三处字体分别覆盖初始和响应式配置：0、-1、正/负 infinity、NaN、denorm_min、min、.01、.49、紧邻 .5 的下界，以及隐藏标签，共 **66** 个拒绝 case。初始配置须在运行子 slot 前拒绝并完整回滚；响应式拒绝须保持 ThemeSnapshot identity、每个旧 Text 的 revision/scene revision/shaped glyphs/measurement/material，以及实际 GlyphInstance 数组，重新同步后仍保持。

另有 **3** 个字体阈值与 DPI 恢复 case：初始精确 .5，以及紧邻其上界、.99、1、1.5、14 和回到 .5；检查实际 glyph font 的 logical_pixel_size 与既有 lround 结果一致。显示比例改为 2 后实际字体 metadata/scene 更新；下界非法更新仍不发布，恢复 14 后可见 glyph、Text identity、Component 数量和 slot 执行次数保持。这里验证 headless 字体 resolver 的实际 2× 栅格结果，不宣称 OS DPI 事件或系统字体验收。

## 本阶段检查

官方 `windows-msvc-headless` preset 的既有独立 `out/build/empty-result-headless`，MSVC **19.51.36256.0** / Ninja Multi-Config / HEADLESS / RECORDING，最多 **4 compiler jobs**。Debug 保持 `/Ob0 /Od /RTC1`，没有扩大线程栈或关闭 iterator debug。

从实际 `ctest --show-only=json-v1` 枚举确定 12 个 executable/target，精确名单保存在 `results.json`：Empty/Result public API、Theme、Component；InputNumber Theme；Theme algorithm/runtime；Text、Icon、Button、Input、Switch features、Popconfirm Theme。分别执行：

```text
$targets = @('rynui_empty_result_public_api_tests', 'rynui_empty_result_theme_tests', 'rynui_empty_result_component_tests', 'rynui_input_number_theme_tests', 'rynui_portable_theme_algorithm', 'rynui_portable_theme_runtime', 'rynui_portable_text_component', 'rynui_portable_icon_component', 'rynui_portable_button_component', 'rynui_portable_input_component', 'rynui_portable_switch_features', 'rynui_portable_popconfirm_theme')
$pattern = '^(rynui\.empty_result_public_api|rynui\.empty_result_theme|rynui\.empty_result_component|rynui\.input_number_theme|rynui\.portable\.theme_algorithm|rynui\.portable\.theme_runtime|rynui\.portable\.text_component|rynui\.portable\.icon_component|rynui\.portable\.button_component|rynui\.portable\.input_component|rynui\.portable\.switch_features|rynui\.portable\.popconfirm_theme)$'
cmake --build out/build/empty-result-headless --config Debug --parallel 4 --target $targets
ctest --test-dir out/build/empty-result-headless -C Debug -R $pattern --output-on-failure -V -j 4
cmake --build out/build/empty-result-headless --config Release --parallel 4 --target $targets
ctest --test-dir out/build/empty-result-headless -C Release -R $pattern --output-on-failure -V -j 4
```

Debug **12/12，15.82 秒**；Release **12/12，9.05 秒**，各包含 66 个拒绝和 3 个阈值/DPI case。完整专项输出见 `debug.log` / `release.log`。原有 R1 布局/图示回归与 retained Button/Input 合同同时通过。本阶段不改共享 Text，因此按授权不重复旧的 148 全量矩阵；历史全量通过记录保留。

从本次新构建的 Debug `rynui_portable_theme_algorithm.exe --write-goldens` 重新生成五份 golden；对 4efa310 **全部字节保持（含 identity）**。`verify-baseline.py` 可在 repo root 重跑，证明 Theme 源码恰好只增加两处 guard、共享 Text/FontRuntime/ThemeRuntime/073 Component/Popconfirm/host source 不变，且只有本次 073 范围有 diff。

clang-format **22.1.3** 检查 **583** 个自有源文件，0 failures；doctor healthy；strict validate **75/75**；`git diff --check` 通过。Gallery、全局文档和父任务 076/077 文件无手工改动。真实 GPU、Linux、系统 IME、本机 OS DPI 与上游插画像素等价均未在本阶段验证。
