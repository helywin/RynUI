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
- `python openspec/changes/090-20261026-add-native-rate/evidence/verify-theme-compat.py`：每份旧字段 3317/3318，合计 16587；34 个既有 IIFE 原文/顺序不变；196 个旧 TokenIdentity 原文/顺序不变。仅增加 rate 与 identity 差异。
- `ctest --preset windows-msvc-headless-debug -R '^rynui\.(rate_|theme_algorithm)' --output-on-failure`：4/4 通过（rate_model/rate_theme/rate_public_api/theme_algorithm）。
- 定向 Theme 测试覆盖独立 seed、inherit=false、Dark+Compact、dirty 分域、最终 .5 字体阈值、非法字体/尺寸拒绝和祖先/后代 preflight 原子恢复。

本记录不把 HEADLESS 场景与合成原生输入事件描述为真实 GPU/原生窗口、Linux、系统 IME/AT 验收。
