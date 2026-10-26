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
