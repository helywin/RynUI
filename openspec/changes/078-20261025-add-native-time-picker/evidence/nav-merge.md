# Time / Breadcrumb / Steps 独立合并验收

2026-10-25，从干净 Time `1116a9931d1c6ea07b25739319570bfd092f0bc3` 执行：

```text
git merge --no-ff --no-commit c083fed5a72f644b491118c6440908e0354d241e
```

保留双亲，不 rebase/cherry-pick。先新增未勾选的 tasks 7.1，完成本记录的检查后才收口。Time 独立 Astra R3 CLEAN 仅适用 `1116a99`，Nav/父集成审查仅适用其原提交；本记录不把它们称为新合并已经审查。新合并的聚焦独立审查由父线程安排。

## 冲突处理与源保留

实际十个冲突文件：五份单行 golden，加 `include/ryn/theme.hpp`、`src/CMakeLists.txt`、`theme_runtime.cpp`、`theme_runtime_types.hpp`、`theme_snapshot.cpp`。

- main 已发布的 TokenIdentity 前缀、原值与顺序全部保留，Time 的 colors/metrics/effects/order 四项追加在 Breadcrumb/Steps 后；未把 worker 旧 ordinal 带到 main。
- `theme_snapshot.cpp` 以干净 main 原文为基准，逐处加入 Time 的构造参数、共享 token 字段、初始化、hash/serialized JSON、equality、静态默认配置 helper 和独立 IIFE。main **30 个原 IIFE 原文/顺序完全相同**，Time IIFE 与 `1116a99` 原文完全相同且单独追加。
- 三者完整 config、构造、accessor、序列化、equality、hash、dirty tracking 和 inheritance 均保留。Time token 继续由既有 shared_ptr 持有，不增加栈额度或改写框架。
- `resolve_theme(const ThemeConfig&, const ThemeSnapshot*)` 公开函数类型不变，保留 Time 已验证的静态默认参数。main 的 `ThemeScope::create_default()`、generic heap descendant preflight、config self guard 与 binding bridge 区域原文相同。
- 所有 main host 与 cleanup/position/input 逻辑保留，只在 Date 后注册 Time。Modal → Table → popup 内容阶段不变；内容 scroll handler 完成后才执行 Time → Date → Dropdown 边界兜底，最近 popup ownership 不变。Nav 原 owned input block cleanup 和所有 hosts 均在。
- Input owner reset 字段、声明、定义各恰好一个，内部 `focused()` 保留；父 Date/Form reset、Nav API/实现/Theme/review、Date Gallery 定义/测试/导航/helper/catalog/README/证据没有自行修改。

可复核脚本 [compare-nav-merge.py](compare-nav-merge.py) 检查 main **121 个指定文件**及 Time/Input **17 个文件**的 Git 内容原文，包含既有 Time 全部 model/host/API/Theme/tests、13 个 pointer 场景及全部 R1/authority reset 回归。共享 WindowComponentServices 去除 Time 的三处接入后，与 main 逻辑文本相同。`rynui.hpp` 保留双方导出；CMake 包含三套组件/Theme 和全部原 targets。

五份 golden 没有选择任一整份旧文件解决冲突；先完成源合并并构建新 generator，再由新 binary 全部重写，随后做下列双向深比较。

## 新同窗口共存例

新增 `time_navigation_merge_tests.cpp`，不改写任何既有 Time/Nav 测试：打开即时 TimePicker，footer 同时放 Breadcrumb 当前项富标题 Input 与 Steps 富说明 Input。实际编辑、同时更新三套 Theme、真实点击小时行、隐藏 Breadcrumb/禁用 Steps 后恢复，最后从 Nav 子 Input 按 Escape 关闭 Time。

断言 slot/row/Input 身份及草稿/session 保持；Nav 当前/展示项仍允许原生 Input；显式隐藏/禁用仅阻断正确子树，恢复后无遗留 block；Time 行值请求恰好一次；Nav 子 Input 的 Escape 仍由父 Time popup 处理，隐藏后 focus/session/caret 正确结束。

## 实际构建与去重组合

实际平台为 Windows / MSVC / `windows-msvc-headless` / Ninja Multi-Config / HEADLESS。官方 configure 成功；Debug 与 Release 的共享 Theme、host 和 [32 个目标](nav-merge-targets.txt) **均实际构建**，build 上限 `--parallel 4`。没有扩栈，没有出现 Windows 栈失败，未需要 CDB。

使用 [固定 44 项清单](nav-merge-tests.txt)，每配置只跑一次同一集合：

| Configuration | 实际结果 | 总时间 |
| --- | --- | --- |
| Debug | **44/44** | **38.66 s** |
| Release | **44/44** | **15.85 s** |

包括 Time 五个 model/API/component/Theme/integration、六个 review、Date form_reset 与 Date/Form component/Theme、Nav component/Theme/model/可运行 API 示例与文档一致性/R1 五项富子控件与 focus、Breadcrumb/Steps 各一个 hidden_blur 生命周期代表例、Theme algorithm/runtime/default/scope、Button、Modal/Spin、Input/focus 和 Core/renderer backend guard，再加新共存例。两个 hidden_blur 仍执行其原有八轮循环与精确 block baseline 断言。没有把 focused 和 combined 集合重叠运行。

初次验收脚本因 VS 环境的 Git PATH、以及 CTest 的括号数量限制停在源比较或选择阶段，尚未执行用例；修正为固定清单后，先验证 inventory 名称/数量全部吻合，并启用 `--no-tests=error`，才得到以上结果。没有将“无测试”当作通过。

等价实际命令（在 x64 VS 环境、UTF-8 控制台）：

```powershell
$evidence = 'openspec/changes/078-20261025-add-native-time-picker/evidence'
$targets = @(Get-Content "$evidence/nav-merge-targets.txt")
$names = @(Get-Content "$evidence/nav-merge-tests.txt")
$pattern = '^(' + (($names | ForEach-Object { [Regex]::Escape($_) }) -join '|') + ')$'
cmake --preset windows-msvc-headless
cmake --build --preset windows-msvc-headless-debug --parallel 4 --target $targets
& ./out/build/windows-msvc-headless/tests/portable/Debug/rynui_portable_theme_algorithm.exe --write-goldens
python "$evidence/compare-nav-merge.py"
ctest --preset windows-msvc-headless-debug -R $pattern --show-only=json-v1
ctest --preset windows-msvc-headless-debug -R $pattern --no-tests=error --output-on-failure
cmake --build --preset windows-msvc-headless-release --parallel 4 --target $targets
python "$evidence/compare-nav-merge.py"
ctest --preset windows-msvc-headless-release -R $pattern --show-only=json-v1
ctest --preset windows-msvc-headless-release -R $pattern --no-tests=error --output-on-failure
```

实际摘要日志保留在 ignored `out/time-nav-merge-{debug,release}-{build,ctest}.log`。没有运行无关完整矩阵或 177 秒日期 Gallery，也没有把父线程的 native inventory/contracts/fullframe/smoke 作为本轮实际运行证据。

## Fresh goldens 双基线

新 Debug binary 写出的 default/dark/compact/dark-compact/compact-dark 均包含三者。脚本深比较所有原 typed scalar、容器类型/形状和嵌套值，只排除顶层 identity 并允许明确的新组件字段：

- 相对 `c083fed`：五份合计 **14,632 个旧 typed leaf** 不变；新增字段集合恰为 `{timePicker}`。
- 相对 `1116a99`：五份合计 **14,447 个旧 typed leaf** 不变；新增字段集合恰为 `{breadcrumb, steps}`，旧 Time 值完整保持。

| 文件 | SHA-256 |
| --- | --- |
| default.json | `2df2b25338c7c27f0730e6e89f94cecf27c93af4a4a54b7476b44454cda899d2` |
| dark.json | `d423527ff65eb1faff4343e15eb3097399b32a87bfea9abb6a7d1f6751db211a` |
| compact.json | `291f2ee99eb178a690cf9bc0072ec2b5e0e5b4971b87a2b0cccde259b1efc04a` |
| dark-compact.json | `7efbd921caecadaef8397618bf0bc82cec3b2612d304fee003a30b1e4cd180f4` |
| compact-dark.json | `7bd890dae3b79629da84e6cc09aa72d365ce0f7e2b1a3a59d7041bab031ef198` |

## 检查与边界

clang-format **22.1.3：646 自有文件，0 failures**；doctor healthy；strict validation **80/80**；diff check、未解决冲突检查通过。Core/renderer configure 与两个配置的 backend guard 均实际通过。

本阶段仅本地英文 Conventional merge commit，双亲为 `1116a99` 和 `c083fed`。不 push、无子任务/PR/archive/父工作树改动。Gallery/目录/README 仅从干净父提交继承；本树没有自行编辑它们。新合并尚未独立 Astra review；Gallery/native fullframe/smoke 由父 079/4.1 统一，原生 GPU、Linux、系统 IME 不作为本轮已验收项。
