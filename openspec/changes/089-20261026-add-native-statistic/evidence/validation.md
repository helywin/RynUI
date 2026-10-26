# Statistic 定向验收

实现提交：`26dffa66`；规划提交：`be484779`。验收均使用当前隔离工作树自己的源码和新建的 `out/build/windows-msvc-headless`，不是主工作树既有 binary。平台通用逻辑在本机 Windows/MSVC 验证，不要求 Linux 重复执行同一合同。

## 命令与结果

在 Visual Studio x64 Developer PowerShell 中使用 Ninja Multi-Config，依赖 BUNDLED；本机实际 MSVC 19.51.36256。本地依赖 source 通过显式 FetchContent source override 读取；验证字体从同一锁定缓存复制至本 worktree 的 `_deps/rynui-validation-fonts`（NotoSans、NotoSansCJKsc、NotoSansArabic、NotoSansHebrew）。未访问远端。

```text
cmake --preset windows-msvc-headless -DFETCHCONTENT_SOURCE_DIR_RYNUI_FREETYPE=D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui_freetype-src -DFETCHCONTENT_SOURCE_DIR_RYNUI_HARFBUZZ=D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui_harfbuzz-src -DFETCHCONTENT_SOURCE_DIR_RYNUI_UTF8PROC=D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui_utf8proc-src -DFETCHCONTENT_SOURCE_DIR_RYNUI_SHEENBIDI=D:/code/RynUI/out/build/windows-msvc-headless/_deps/rynui_sheenbidi-src
cmake --build --preset windows-msvc-headless-debug --parallel 2 --target rynui_statistic_model_tests rynui_statistic_theme_tests rynui_statistic_component_tests rynui_statistic_public_api_tests rynui_portable_theme_algorithm rynui_timeline_review_tests
cmake --build --preset windows-msvc-headless-release --parallel 2 --target rynui_statistic_model_tests rynui_statistic_theme_tests rynui_statistic_component_tests rynui_statistic_public_api_tests rynui_portable_theme_algorithm rynui_timeline_review_tests
ctest --preset windows-msvc-headless-debug -R "rynui\.(statistic_|portable.theme_algorithm|timeline_review_(fill|document_hit))" --output-on-failure
ctest --preset windows-msvc-headless-release -R "rynui\.(statistic_|portable.theme_algorithm|timeline_review_(fill|document_hit))" --output-on-failure
```

- Debug：13/13 通过，1.93 秒，见 [debug-tests.txt](debug-tests.txt)。
- Release：13/13 通过，1.08 秒，见 [release-tests.txt](release-tests.txt)。本配置从零构建 211 个 build actions（Ninja 计数含额外进度槽显示 `/212`），见 [release-build.txt](release-build.txt)。
- Configure 成功，见 [configure.txt](configure.txt)。保留 HarfBuzz 的 CMake 支持提示、FreeType 已有 C4819 和旧组件已有 warning；不声明无警告。
- 10 个 Statistic 项目覆盖 model、Theme、retained、formatting、clips、lifecycle、gates、boundaries、public API、docs example；另有共同 Theme algorithm 与父 Timeline fill/document_hit 2 个直接相关回归。

```text
out/build/windows-msvc-headless/tests/portable/Debug/rynui_portable_theme_algorithm.exe --write-goldens
python openspec/changes/089-20261026-add-native-statistic/evidence/compare-theme.py
python openspec/changes/089-20261026-add-native-statistic/evidence/verify-stack.py
python scripts/format-code.py --check --clang-format "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe"
```

实际 Python 为 `C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe`。5 份 fresh goldens 深比较：16,587 个旧 typed JSON 叶值保持，36 个旧 IIFE 原文和顺序保持，196 个旧 TokenIdentity 顺序保持，只新增 Statistic 和 identity 变化；见 [theme-comparison.txt](theme-comparison.txt)。8 个新增 Debug/Release PE 的 `SizeOfStackReserve` 均为 1048576，见 [pe-stack.txt](pe-stack.txt)。clang-format 22.1.3 全仓 736 个自有源文件检查，0 failures，见 [format.txt](format.txt)。

## OpenSpec 工具版本边界

仓库生成 skills 与要求为 1.14.0，但当前 Windows PATH 实际全局入口 `C:/Users/jiang/AppData/Local/pnpm/openspec.ps1` 固定到 pnpm 包 `@fission-ai/openspec@1.13.1_@types+node@24.10.1`；`openspec --version` 实测 1.13.1，WSL 亦解析到相同 Windows 入口。已检查本机全局 pnpm 包和缓存，未找到 1.14.0。遵守 worker 不访问远端的限制，没有在线升级，也未修改 ROOT 或全局工具。

当前已运行：`openspec doctor --json` 健康，`openspec validate --all --strict --no-interactive` 88/88 通过；实际版本明确为 1.13.1，不能当作指定 1.14.0 的证据。见 [openspec-version.txt](openspec-version.txt)、[doctor-1.13.1.json](doctor-1.13.1.json)、[strict-1.13.1.txt](strict-1.13.1.txt)。已向父线程请求已安装的 1.14.0 入口；`tasks.md` 3.2 在指定版本补验前保持未勾选。

## 限制与移交

首版仅 Statistic；Timer、Countdown、Countup、数值动画、locale 国际化均不覆盖，没有类型占位。formatter 返回 String，富输出由 retained typed value slot 表达。浮点若 to_chars 产生指数形式则保持原样，需要精确十进制位时使用 String；precision>100 明确拒绝。

未运行历史 full suite、长 Gallery、原生窗口、GPU、Linux、系统 IME/AT；不将 HEADLESS 路由/场景证据称为这些平台或服务已通过。README、docs/component-completion.md、Gallery、support-overlay/generated catalog 保持基线内容。worker 只保存本地英文阶段提交，父负责 Astra 审查、main 集成/push 与远端核对；worker 未执行远端、PR、archive 或 children 操作。
