# 090 本地实施证据

## 基线与边界

- worktree：`C:/Users/jiang/.t3/worktrees/RynUI/t3-c74e4a3d`，分支 `t3/add-native-rate`；基线 `8056de67953ee73015c7559f1782033d85e08573`。
- 只做隔离工作树本地实现/提交；无远端、push、PR、archive、children、ROOT/Gallery 改动。父线程负责审查与 main 集成。
- 读取锁定 Ant Design 6.6.5 本地源，`git -C D:/code/RynUI/out/upstream-ant-design-6.6.5 rev-parse HEAD` 返回 `4a39f54842eade4e565ab336ef6097cd7e723cdd`。
- `openspec` PATH 默认仍为 1.13.1；本次使用 `node C:/Users/jiang/AppData/Local/pnpm/global/v11/10734-18e1afefebdf56bc-0/node_modules/.pnpm/@fission-ai+openspec@1.14.0/node_modules/@fission-ai/openspec/bin/openspec.js`，实测版本 1.14.0。下文简写为 `node $osc`。

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
