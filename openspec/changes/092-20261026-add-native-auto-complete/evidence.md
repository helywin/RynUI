# AutoComplete 实施证据

## 基线与规划

本树 `C:/Users/jiang/.t3/worktrees/RynUI/t3-081ff944` 从 `83d26e7a16a0590c2d52867296a20dd3cf4aee78` 开始，规划 commit `27cdcbe8`。干净阶段显式 merge 父指定 `6118f67728aede9985867377b122274ac86f31d7`，merge commit `2c6dbba2a005a39ef487714bf4b0198cf353969f`。没有 fetch/rebase/push/PR/archive/children，没有手工创建 branch/worktree。app 当前 branch 为 `t3/add-native-autocomplete`。

锁定上游只读 `D:/code/RynUI/out/upstream-ant-design-6.6.5` HEAD `4a39f54842eade4e565ab336ef6097cd7e723cdd`；阅读 AutoComplete.tsx、index.en-US.md、__tests__/index.test.tsx、demo/basic/options/non-case-sensitive。Context7 查得当前说明作补充，行为以指定本地版本为准。

## 阶段 2：模型与 Theme

Windows MSVC 19.51.36256.0、Ninja Multi-Config、`windows-msvc-headless`、BUNDLED，`--parallel 2`。配置 helper `out/092-msvc.ps1` 引入 VSDevShell，仅引用 ROOT/out 的四个锁定第三方 source；字体复制到本树，所有输出/cache/log 都在本树。配置第一次因 PowerShell 将 HarfBuzz 的 CMake warning 当作终止错误而退出；调整外部命令错误流处理后配置 exit 0。没有修改全局工具或第三方源。

实际构建 targets 为 `rynui_auto_complete_model_tests rynui_auto_complete_theme_tests`，Debug/Release 各 194 个 Ninja 步骤、exit 0。`ctest --preset windows-msvc-headless-{debug,release} --parallel 2 --output-on-failure -R '^rynui\.auto_complete_(model|theme)$'` 均 2/2，Debug 0.24 s，Release 0.07 s。覆盖自由输入、默认不过滤、predicate、禁用跳过、受控权威、backfill、key 重排、非法 key/predicate 拒绝；Theme 覆盖 recipe、独立 algorithm、inherit、hash/equality、material/metrics dirty、font round 上下限、非法字体和 descendant preflight 原子性及恢复。

两配置 Theme 测试各 fresh 生成五份 goldens；`tests/verify_auto_complete_theme_baseline.py out/092-goldens-Release` 通过：Avatar 集成基线的 37 个旧 IIFE 原文/顺序（含原始 36 个）、198 个旧 TokenIdentity ordinal（原始 196 + Avatar 2）和五份旧 JSON 全部嵌套 typed leaves 不变，只追加 AutoComplete。五份 repo goldens 来源为本树 Release fresh 输出。

阶段日志：`out/092-configure-Debug-attempt1.log`、`out/092-configure-Debug.log`、`out/092-stage2-test-{Debug,Release}.log`、`out/092-goldens-{Debug,Release}.log`、`out/092-stage2-baseline.log`、`out/092-stage2-format.log`。初期 helper 使用固定 build 日志名，Debug 初建日志后来被下一阶段覆盖；194 步的 Debug 首建来自本次工具输出，不把现存 `out/092-build-Debug.log` 当作那次首建原始日志。后续 build/test 已采用时间戳且收口复制原始日志入本 change。现有第三方 C4819 及自有既存 warning 保留。OpenSpec 显式 1.14 doctor healthy、strict 90/90，diff check 通过；format 使用 clang-format 22.1.3 显式路径，已跟踪 731 个文件 0 failures，新增文件另已直接格式化（收口时纳入完整检查）。

此阶段没有把尚未链接的原生 host 计作完成；原生事件、Form、popup、生命周期和示例仍待后续阶段。

## 阶段 3.1：真实编辑与建议

模型/Theme commit `3eea6384` 后接入真实 AutoComplete host，复用 Input/editor、InputRef/reset hook、retained surfaces、window layer 和 Popover 祖先定位。不是 stub，也不构造 Virtual DOM。新增 `examples/auto_complete_example.hpp`，由文档入口和实际运行的 example case 共用。

Debug/Release fresh 编译 `rynui_auto_complete_{model,theme,component}_tests` 后，定向 CTest 各 7/7（model、theme、flow、controlled、retention、form、example）。实际事件覆盖自由 String、onChange/onSearch/onSelect 顺序及同值选择、键盘/Pointer/clear/Tab、受控值与 open 拒绝/接受、空 options 与过滤、typed Input props、rich label 下原生 Input 草稿/preedit/完整 editor 身份保留、移除 key 的旧 session/ref 失效、主输入 IME 上下/Enter 门禁，以及真实 Form String/control(ref) 同值 preedit/backfill reset 与 readOnly。示例实际挂载输入/reset。清理检查 nodes、glyph scene、surfaces、interactions、owned blocks 和 deadline。

日志：`out/092-native-first-tests-Debug.log`（初次四项通过）、`out/092-stage3-test-{Debug,Release}.log`（最终七项）及 `out/092-build-{Debug,Release}-*.log`。中间首次 component 测试编译发现 snapshot bool 误写函数调用，已修复后 fresh 编译通过；没有把失败计作验收。popup 裁剪/子 popup、边界和同步回调重绑定的进一步验证属于下一阶段。

## 阶段 3.2：popup、输入边界与生命周期

真实编辑阶段 commit `521daf02`。本阶段增加 clips、children、boundaries、lifetime、idle、theme_runtime 六个 CTest case，AutoComplete 合计 13 项。包含有限视口缩小至 1×1 后恢复、全部十行滚动可达、实际 glyph/标记 fill/hit/focus outline clip、受控父 Popover 内的 AutoComplete 与子 Popover/Select Escape/wheel 所有权、Modal/Spin/hidden/window active、行 capture 与延迟 up、七类回调（change/search/select/clear/open/focus/blur）同步销毁并重绑定 ref、旧 Form reset adapter/session/完整 ComponentId 代际拒绝、外部 owned block 保留、聚焦 rich Input 删除/替换、缩短列表保留 row 和 idle 无持续帧。Form reset 还实际注入候选列表，普通建议变更验证同一 session stamp 和 preedit 保留。

发现并修复：挂载期祖先 popup 暂时 inactive 时不抢先 suppress，首次完整布局补算可见性；删除/替换前先阻断旧子树，允许 blur 销毁整个 AutoComplete 后立即按完整 id 退出；边界回调按 key 解析当前 options，避免列表缩短后的旧 index；后代窗口层优先拥有 Escape；复制 authority reader 后重查原 id/editor 再发布。原生 Input 的 IME Escape 取消其自身 composition，建议逻辑不消费、不关闭 popup、不改变 accepted 值；spec 已明确此边界。

收口针对测试 19 项：AutoComplete 13 + portable Theme algorithm/runtime 2 + 受影响 Popover wheel/children/covered/clips 4。两配置均为本树 MSVC fresh 编译/链接后的运行：Debug 19/19，CTest real 2.52 s；Release 19/19，0.93 s，exit 0，最多 2 jobs。Debug 最后核心源码构建 `out/092-build-Debug-20261026-181601.log`，共享回归目标新建 `out/092-build-Debug-20261026-181916.log`；Release `out/092-build-Release-20261026-181723.log`；测试原始日志 `out/092-test-Debug-20261026-182050.log`、`out/092-test-Release-20261026-181907.log`。后续文档收口不改变这些已测 source blobs。

中间失败日志保留在 out：最初零 viewport 试验不符合宿主合同，改为合法有限视口；nested 可见性与 rich blur 销毁失败已由最终对应真实事件验证修复。曾在一次 integration 测试编译失败后误跑旧 executable，结果含 unknown case，`out/092-stage32-test-Debug.log` 仅为失败诊断，不计作 fresh 验收；最终通过记录为上述带时间戳的日志。没有以测试设计、源码阅读或旧 binary 代替最终通过结果。

## 阶段 4：收口与交接

最终被测 C++ 实现 commit 为 `348abf6861b5328850ab798a9986b3c5e14dc4d2`；随后只更新验证脚本与文档/证据，不改变已测 C++、CMake 或示例 source blobs。当前 app 绑定 branch `t3/add-native-autocomplete`，路径仍为本文开头的独立 worktree。实现阶段提交依次为：

- `27cdcbe8ed1c480498443a8976d29f1b75d585df`：规划。
- `2c6dbba2a005a39ef487714bf4b0198cf353969f`：精确合并 verified Avatar main `6118f67728aede9985867377b122274ac86f31d7`。
- `3eea638499d9dae98b854f08ede3afae13e72adc`：typed model 与完整独立 Theme。
- `521daf022dafd711df32cb8596106538562b38d7`：真实 Input、keyed/rich 建议、事件、Form reset 与示例。
- `348abf6861b5328850ab798a9986b3c5e14dc4d2`：popup/clip/输入边界/生命周期收口。

最终命令（cwd 必须为本 worker worktree，helper 仅使用显式 VS MSVC 环境与本树输出）：

```text
powershell -NoProfile -ExecutionPolicy Bypass -File out/092-msvc.ps1 -Action build -Config Release -Final
powershell -NoProfile -ExecutionPolicy Bypass -File out/092-msvc.ps1 -Action test -Config Release
powershell -NoProfile -ExecutionPolicy Bypass -File out/092-msvc.ps1 -Action build -Final
powershell -NoProfile -ExecutionPolicy Bypass -File out/092-msvc.ps1 -Action test
powershell -NoProfile -ExecutionPolicy Bypass -File out/092-msvc.ps1 -Action goldens
powershell -NoProfile -ExecutionPolicy Bypass -File out/092-msvc.ps1 -Action goldens -Config Release
C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe tests/verify_auto_complete_theme_baseline.py out/092-goldens-Debug
C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe tests/verify_auto_complete_theme_baseline.py out/092-goldens-Release
C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe out/092-verify-pe.py
C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe out/092-final-verify.py
```

`-Final` 构建的实际 targets 为 `rynui_auto_complete_model_tests rynui_auto_complete_theme_tests rynui_auto_complete_component_tests rynui_auto_complete_integration_tests rynui_portable_theme_algorithm rynui_portable_theme_runtime rynui_popover_integration_tests`。测试正则为 `^rynui\.(auto_complete_.*|portable\.theme_(algorithm|runtime)|popover_(wheel|children|covered|clips))$`。不运行完整 CTest/Gallery/native。

最终 Debug/Release Theme executable 分别 fresh 生成五份 golden。验证脚本同时以原始 `83d26e7a` 和 Avatar `6118f677` 为证据：36 个原始 IIFE 原文/顺序、196 个原始 ordinal 不变；包含 Avatar 后为 37/198；仅 AutoComplete 在其后追加一个 recipe 与五个 identity。递归比较要求 JSON 每个叶子的 Python 类型及值完全相同（区别 bool/int/float），两套旧 typed leaves 全部保留。五份 fresh Debug == fresh Release == repository 字节相同，SHA256 原始结果见 artifact 日志。

八个本树新建 AutoComplete test PE（model/theme/component/integration × Debug/Release）均为 PE32+，stack reserve `1048576` bytes，没有 `/STACK` 扩大。clang-format 22.1.3 全部 742 个 owned C++/HLSL 文件 0 failures；显式 OpenSpec 1.14 doctor healthy、strict 90/90 与 diff check 通过。README、completion、Gallery、091 frame tests、support-overlay 和 generated catalog 相对精确 Avatar 基线无 diff。未导入移动 main 或 089/090 WIP。

最终原始 build/test 日志已按字节复制到 [evidence/windows](evidence/windows/)；其中 Debug source build 和共享回归目标 build 分开记录。fresh golden 的基线检查、PE 与 artifact/hash 输出也保存在该目录。所有本机缓存、测试 exe 与额外诊断日志仍在本树 out；没有运行 ROOT 程序或写 ROOT/out。

首版限制保持 proposal/docs 所列：网络异步、分组、虚拟化、动画、RTL/Web、任意 editor/popupRender 未实现也未暴露假 props。未验收 native GPU、Linux、系统 IME、AT、完整 Gallery/native startup。当前结果是 worker 首版实现与本树 headless 验证；等待父 fresh Astra 审查和 main 集成，不创建 PR/archive，不推 worker branch。

## Astra R1：实际复现与修复

本轮从 clean `bca1acafde1c7486d549bc9104a3e2f836b937ba` 继续，没有导入移动 main 或其它 WIP。首轮修复 commit 为 `b3c9626d4280ff5f763c9fa08dbd6e4547d87477`（`fix(auto-complete): guard lifetimes and popup input transactions`）；P1 审计补测后增加 `ab0cd6002f5f91241df909225b3c00f41a06f38d`（`fix(auto-complete): stop superseded boundary refresh delivery`），它是最终被测 source commit。后续证据提交不改已测 C++/CMake/example blobs。仍为同一个 app 绑定 worktree/branch；Avatar 精确 merge 及原 13 个 AutoComplete case 源码均保留。

先只添加独立 review harness/CMake cases，在未修改原实现的基线上以本树 MSVC Debug 编译、运行五个独立进程，结果 **0/5**；不是根据源码推测失败。初始 harness 之后扩充，原复现入口保留在每组测试开头。

| R1 问题 | 修复前实际结果 | 修复后直接断言 |
| --- | --- | --- |
| P1 首次关闭富 Select，回调销毁外 owner | `auto_complete_review_mount` SegFault，4.03 s | owner、私有 Input/popup/list/row/label region、affix、Prop reader/filter、首次 count formatter/autoFocus 退役及 close 回调重绑，停止旧代际并清理资源；富槽内 options/value/open/size 更新保留 rich 身份并首帧生效 |
| P2 rich Input Tab 锚点 | `rich Tab/ShiftTab did not traverse relative to the main Input` | Before→主 Input→After 的相邻顺序；rich Input/嵌套 Popover Input 两向 Tab、程序焦点离开后代关闭，最终 popup hit 消失 |
| P2 实际零尺寸 | `zero allocated geometry retained popup/focus/session/preedit` | 真实零高/零宽取消 focus/session/preedit、ref 拒绝、同值 reset；正尺寸不自动恢复建议，显式 authority/Down 恢复；初始 open Modal/defaultOpen 不误抑制；隐藏 popup 的 glyph 不进入最终 ordered draw commands |
| P2 root AC→子 Select 同帧定位 | 首帧先抛 `Rounded effect geometry is invalid`，尚未到自定义 anchor 断言 | 无外层 Popover 的直接 root 组合，首帧/父移动/父建议滚动后 child anchor、实际 fill、glyph clip、hit 同帧匹配 |
| P2 消费命令的 keyboard modality | `consumed Down did not deliver keyboard modality before keyup` | 只送 keydown 即为 keyboard/focus_visible，并有实际 outline；Up/Enter/Escape 同样交付；open 回调销毁 owner/private child、重绑 ref 或 disabled/readOnly 后不触碰旧模型/新代际 |

局部守卫记录完整 owner/private ComponentId/NodeId、直接父关系、主 Input interaction/editor；每次同步用户代码后重取状态。外部连接 Scope 独立持有至首次 synchronous read/apply 返回，并随原 owner dispose。模型 predicate 在独立副本上计算，逐次检查代际；富槽更新用 revision/pending 合并行协调。没有添加吞错 catch 或全局生命周期重构。唯一 Input Form reset hook 保持原入口。

共享改动只有三处行为入口：LayoutStyle adapter 的可选局部 guard；WindowComponentServices 在所有 prepare（含后执行的 Input formatter）后、LayoutEngine 遍历前复核 AutoComplete 私有结构；Select 的逐 owner 定位与 Popover 祖先解析相接并加递归 guard。没有交换全局 host 顺序。DateTime↔Popover 反向嵌套代表、现有 AC children 组合及 root AC→Select 均实际运行。Input/Form 实现源码未修改。

中间补测首次 formatter 删除主 Input 时曾报告 `NodeId is stale or invalid`，原始诊断保留；增加遍历前复核后通过。曾把保留 glyph 存储直接当成提交可见性断言，已改为检查最终 ordered draw commands；readOnly 仍允许原生焦点，修正该 fixture 的错误无焦点预期。新增 fixture 的编译笔误、HEADLESS target 名称和 CTest 过深 regex 的命令错误均未计作通过；最终使用下列实际 37 项集合，`--no-tests=error`。没有在失败 build 后运行旧 binary 计验收。

P1 审计补测还实际复现：子 Select 首次关闭回调同步把父 open 回显为 true 时，旧 refresh 在嵌套更新后仍继续设置 row block，snapshot/paint record 看似可见却不可交互。`092-R1-open-echo-before-Debug.log` 是 b3c 实现上的真实失败，不把最初只有 paint record 存在的不足断言当作通过证据；加强为完整 row 输入资格后复现。增加局部 refresh 代际，在每次同步交付后停止旧一轮更新；最终同一用例通过。

最终使用同一正式 preset、MSVC、Ninja Multi-Config，build/CTest 均 `--parallel 2`；Debug 和 Release 顺序执行。两配置在最终 refresh 代际修复后重新编译/链接所有指定 targets，再运行：**Debug 37/37，14.29 s；Release 37/37，6.17 s**。集合为原 19 项（AC 13 + portable Theme 2 + Popover 4）+ 新 review 5 + 直接共享代表 13：DatePicker component/wheel/form_reset 3、TimePicker component/reset/focus 3、Popover positioning/focus 2、portable Input component/actions/count 3、Form component 1、portable Select component 1。不是完整 CTest 或 native/GPU 验收。

最终命令的 targets 与单层 regex 原样保存在 [092-r1-final.ps1](evidence/windows/r1/092-r1-final.ps1)，其调用的 [092-msvc.ps1](evidence/windows/r1/092-msvc.ps1) 同样按字节保存供复核；执行 cwd 始终是本 worker worktree，仅输出到本树 out。实际日志见 [r1 原始证据](evidence/windows/r1/)：

- 修复前：`092-build-Debug-20261026-185812.log`、`092-R1-before-Debug.log`。
- 最终 Debug：共享目标首编 `092-build-Debug-20261026-193459.log`，最终 review 首编 `092-build-Debug-20261026-195450.log`，最终 C++ 重编及所有指定目标 relink `092-build-Debug-20261026-195625.log`，`092-R1-final-test-Debug.log`。此前 autoFocus 阶段的 194253/194454 build 日志也保留。
- 最终 Release：主体重编 `092-build-Release-20261026-193811.log`，最终 C++/review 重编及 relink `092-build-Release-20261026-195658.log`，`092-R1-final-test-Release.log`。此前 autoFocus 阶段的 194515 build 日志也保留。

Theme/金样未修改，不重新生成。以 `tests/verify_auto_complete_theme_baseline.py design-tokens/ant-design/6.6.5/golden` 复核本仓五份已有金样：36 原 IIFE/196 ordinal 及 Avatar 后 37/198 原文/顺序不变，全部旧 typed leaves 类型和值不变，仅此前 AC 追加。脚本输出改称 supplied，避免把本轮复核误称 fresh 生成。原 font round 上下限、descendant preflight 原子性测试包含在最终 Theme case。10 个 AC test PE（含 review，5×Debug/Release）实际均为 PE32+、stack reserve 1048576 bytes。

最终 clang-format 22.1.3 检查 743 owned 文件，0 failures；显式指定安装入口的 OpenSpec **1.14.0** doctor healthy、strict **90/90**，diff check 通过。README/completion/Gallery/support-overlay/generated catalog、Theme/金样、Avatar、Input/Form 实现及原 AC component/integration tests 相对 bca1 未改。没有 fetch/push/PR/archive/children，没有运行 ROOT 程序或写 ROOT/out。所有系统专属边界仍未验：native GPU、Linux、系统 IME、AT；本树结果等待父 fresh Astra R2 与 main 集成，不据此宣称 R2 CLEAN。

[artifacts.log](evidence/windows/r1/artifacts.log) 记录最终 source commit、完整 branch/path、十个实际 PE 的 SHA256/栈、五份未改仓库金样与全部本轮原始证据文件的 SHA256；由同目录只读 `verify_artifacts.py` 在本树实际核验。该脚本同时拒绝保护路径改动及最终 source commit 之后的 src/include/tests/examples 变化。
