# 090 本地实施证据

## 基线与边界

- worktree：`C:/Users/jiang/.t3/worktrees/RynUI/t3-c74e4a3d`，分支 `t3/add-native-rate`；基线 `8056de67953ee73015c7559f1782033d85e08573`。
- 只做隔离工作树本地实现/提交；无远端、push、PR、archive、children、ROOT/Gallery 改动。父线程负责审查与 main 集成。
- 读取锁定 Ant Design 6.6.5 本地源，`git -C D:/code/RynUI/out/upstream-ant-design-6.6.5 rev-parse HEAD` 返回 `4a39f54842eade4e565ab336ef6097cd7e723cdd`。
- `openspec` PATH 默认为 1.13.1；初期使用已安装的 1.14.0 pnpm/v11 入口，最终按父给定路径使用 `& 'C:/Program Files/nodejs/node.exe' 'C:/Users/jiang/AppData/Local/pnpm/global/v11/571d8138cbe6cca8ed77baade632db40d9ff691763fbc5f585da857bbb3b1d1f/node_modules/@fission-ai/openspec/bin/openspec.js'`，实测版本 1.14.0。下文简写为 `node $osc`。未安装或升级全局工具。

## 阶段 1

- `node $osc context --json` / `list --json`：根为当前隔离工作树。
- `node $osc doctor --json`：healthy=true；`node $osc validate --all --strict --no-interactive`：88/88。
- MSVC 19.51.36256.0，官方 `windows-msvc-headless`，Ninja Multi-Config；复用 ROOT 既有锁定第三方源（只读 source）与复制到本地 build 的验证字体，build/cache/objects 均在隔离树。`cmake --preset windows-msvc-headless -DFETCHCONTENT_SOURCE_DIR_RYNUI_FREETYPE=D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui_freetype-src -DFETCHCONTENT_SOURCE_DIR_RYNUI_HARFBUZZ=D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui_harfbuzz-src -DFETCHCONTENT_SOURCE_DIR_RYNUI_UTF8PROC=D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui_utf8proc-src -DFETCHCONTENT_SOURCE_DIR_RYNUI_SHEENBIDI=D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui_sheenbidi-src` 成功。
- `cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_rate_model_tests rynui_rate_theme_tests rynui_rate_public_api_tests rynui_theme_algorithm_tests` 成功。
- `out/build/windows-msvc-headless/tests/Debug/rynui_theme_algorithm_tests.exe --write-goldens`：生成全部五份 fresh JSON。
- `python openspec/changes/090-20261026-add-native-rate/evidence/verify-theme-compat.py`：每份旧字段 3317/3318，合计 16587；36 个既有 IIFE 原文/顺序不变；196 个旧 TokenIdentity 原文/顺序不变。仅增加 rate 与 identity 差异。
- `ctest --preset windows-msvc-headless-debug -R '^rynui\.(rate_|theme_algorithm)' --output-on-failure`：4/4 通过（rate_model/rate_theme/rate_public_api/theme_algorithm）。
- 定向 Theme 测试覆盖独立 seed、inherit=false、Dark+Compact、dirty 分域、最终 .5 字体阈值、非法字体/尺寸拒绝和祖先/后代 preflight 原子恢复。

本记录不把 HEADLESS 场景与合成原生输入事件描述为真实 GPU/原生窗口、Linux、系统 IME/AT 验收。

## 阶段 2

- `cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_rate_model_tests rynui_rate_theme_tests rynui_rate_public_api_tests rynui_rate_component_tests rynui_rate_lifecycle_tests rynui_theme_algorithm_tests rynui_timeline_review_tests rynui_popover_integration_tests rynui_notice_review_tests` 成功。
- `ctest --preset windows-msvc-headless-debug -R '^rynui\.(rate_|theme_algorithm$|timeline_review_document_hit$|popover_clips$|notice_review_clip_contract$)' --output-on-failure`：9/9 通过，2.47 秒。
- rate_component：真实 PointerRouter/FocusManager 事件、单一 Tab 入口、左右键/Home/End/Enter/Space、半选/清空、keyboard=false 仍能鼠标编辑、受控拒绝、count 缩减/恢复、非法值恢复、三尺寸文本/图标的实际 glyph clip、有限宽/短高/零尺寸、逐项 Tooltip/readOnly，以及富标题的真实 glyph/quad/native Input hit/focus。
- rate_lifecycle：隐藏不留 20dp 内容和 9dp gap、禁用/零 count/Spin/Modal/窗口失活取消 hover/capture/focus、count 增长与缩减保留 ComponentId、富标题 Input 保留 editor/draft/ref/session、主题变化保留内容、hover/onChange/native blur 同步卸载与同一 InputRef 重绑、旧 session 拒绝、owned blocks 清理、静止/空组件无空转 deadline。
- 实测发现原共享命中总 clip 在无 Modal/Notice 时使用 document clip，截断了既有 Tooltip 窗口层原生子控件。改为窗口总范围，并继续通过 `scene_composer_.rebuild(hit_clip, clip)` 对普通文档节点单独施加 document clip。Rate 有限文档命中不越界与窗口层原生命中均由实际 hit records 验证；另通过 popover_clips、notice_review_clip_contract、timeline_review_document_hit 三个定向回归。
- 金样兼容脚本已覆盖包括 empty/result 赋值式 IIFE 在内的全部 36 个旧 IIFE；196 个旧 identity 与 16587 个旧 typed 叶字段仍不变。

## 阶段 3：最终本机验收

实施源码提交：`817951e06873270c801717c11709aba4178cd3b7`。其后仅收拢本 change 文档和验证脚本，不增加组件功能。

- `cmake --build --preset windows-msvc-headless-release --parallel 2 --target rynui_rate_model_tests rynui_rate_theme_tests rynui_rate_public_api_tests rynui_rate_component_tests rynui_rate_lifecycle_tests rynui_theme_algorithm_tests rynui_timeline_review_tests rynui_popover_integration_tests rynui_notice_review_tests`：首次 Release 构建成功；日志 `out/rate-release-build.log`，208 个 Ninja 步骤，9 个定向可执行文件完成链接。
- `out/build/windows-msvc-headless/tests/Release/rynui_theme_algorithm_tests.exe --write-goldens` 后再次运行 `python openspec/changes/090-20261026-add-native-rate/evidence/verify-theme-compat.py`：全部五份 fresh 金样与 Debug 输出相同，16587 个旧 typed 叶字段深比较无差异，36 个旧 IIFE 和 196 个旧 identities 原文/顺序无差异。
- `ctest --preset windows-msvc-headless-release -R '^rynui\.(rate_|theme_algorithm$|timeline_review_document_hit$|popover_clips$|notice_review_clip_contract$)' --output-on-failure`：9/9 通过，1.34 秒。实际 CTest 日志为 `out/build/windows-msvc-headless/Testing/Temporary/LastTest.log`。
- Debug 与 Release 的同一 9 项定向检查均通过；未执行历史 full suite 或长 Gallery，未声称其他平台/原生窗口/GPU 验收。
- `python openspec/changes/090-20261026-add-native-rate/evidence/verify-pe-stack.py`：18 个实际 Debug/Release MSVC x64 PE 的 `SizeOfStackReserve=1048576`，未扩大栈；新测试的大 ThemeConfig/Snapshot 放在 heap。
- `python scripts/format-code.py --check --clang-format 'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe'`：737 个自有源文件，clang-format 22.1.3，0 failures。
- `node $osc doctor --json`：当前隔离根 healthy=true；`node $osc validate --all --strict --no-interactive`：88 passed / 0 failed；`git diff --check` 与 `git diff --cached --check` 通过。

交接范围：Rate API/model/host、Theme 全链、窗口服务注册和窗口层命中总 clip、CMake、5 份金样、Rate 专项测试和文档。未改 README、component-completion、Gallery、support overlay/generated catalog、renderer ABI 或 DateTime/Form reset hook。字符是纯文本/图标值，按 index 的函数在该项初建时生成；富原生内容通过只挂载一次的 TooltipTitle slot 提供。count 上限 1000，隐藏/缩减保留已建项以保持身份。真实 GPU、原生窗口、Linux、系统 IME/AT 以及父 main 集成/审查仍由父线程按其工作流处理。

## 父合并前补正

父线程随后发现 Rate 字号上限与 FontRuntime 不一致；原 `0190d4b1` 上已实际复现，最小补正及本次 Debug/Release 专项结果见 [font-limit-correction.md](font-limit-correction.md)。上述 9/9 是原首版源码的验收，本补正未重复该矩阵，也尚未接受独立 Astra 审查。

## Astra full R1 响应

父随后转交的首次 full R1 为非 CLEAN，包含四项 P2。四项在 cea1552c 上全部真实复现并修复，before/after、原生连续鼠标/Tab/IME/outline/lifetime 证据及 7/7 Debug/Release 定向结果见 [astra-r1-response.md](astra-r1-response.md)。本 worker 完成后停待 fresh R2，不将本地通过描述为 Astra CLEAN。

## Astra R2 响应

父随后转交 R2 的动态追加焦点顺序 P2。在 0df5c99b 上真实复现 count 0→1 跳过 Input，已最小重排完整 retained 后代，验证增长/隐藏恢复/多个 owner 与 foreign 隔离；本次每配置一次 7/7 定向结果及不变量证据见 [astra-r2-response.md](astra-r2-response.md)。完成后停待父 fresh R3。
