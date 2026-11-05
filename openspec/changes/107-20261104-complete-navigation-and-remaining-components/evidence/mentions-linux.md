# Mentions Linux 验收

2026-11-05；仅 16.1/16.2。基线 `2777cdc3e7076247aadeac66f28088fbaeb4eb49`，Ant Design 6.6.5 锁定 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd`。实现为保留的原生 TextArea、caret 窗口层、稳定 key 建议、应用提供的可取消异步和独立 Mentions Theme；未改写 Form 核心。

## 平台通用合同

Linux GCC、`Ninja Multi-Config`、BUNDLED；preset `linux-gcc-headless`，build/test presets `linux-gcc-headless-debug` 与 `linux-gcc-headless-release`。网络 TLS 失败时复用既有 ignored `out/` 中 SHA 锁定的源码/字体缓存，不修改依赖锁。

对 Debug/Release 分别执行（`<config>` 为 `debug`、`release`）：

```sh
cmake --build --preset linux-gcc-headless-<config> --target rynui_mentions_tests rynui_mentions_theme_tests rynui_mentions_public_api_tests rynui_mentions_gallery_tests rynui_theme_algorithm_tests rynui_theme_runtime_tests rynui_auto_complete_complete_tests rynui_input_number_component_tests rynui_portable_input_component rynui_portable_text_area rynui_portable_input_actions rynui_portable_input_count rynui_portable_input_variant rynui_portable_input_public_api rynui_portable_theme_scope_component -j2
ctest --preset linux-gcc-headless-<config> -R '^rynui\.(mentions_|auto_complete_complete_|input_number_component$|theme_(algorithm|runtime)$|portable\.(input_component|text_area|input_actions|input_count|input_variant|input_public_api|theme_scope_component|backend_boundaries)$)' -j2 --output-on-failure
```

结果：Debug **35/35**、Release **35/35**，最终失败 **0**。各含 20 个 Mentions 检查及 15 个受影响回归，实际编译 Gallery definition/page contracts 和公开 Form/ref 示例。覆盖 Unicode/bidi/grapheme 选区、换行/折行/滚动/祖先平移定位、prefix/filter/event 顺序、受控拒绝与重入、准确文本/selection、maxLength、Core 剪贴板/组合输入、popup 子焦点/Tab/Escape、10,000 项窗口化/富 Input 草稿、边界滚轮、异步乱序/取消/重复/跨线程误投递、失败退休、过期引用/票据、同值 Form reset、隐藏/禁用/只读/阻塞/零尺寸/失活及 idle 清理。

五份主题 golden 均新增 Mentions；基线的 **60 个顶层字段除整体 identity 外逐字段相同**，旧 TokenIdentity 序号不变。独立 seed/algorithm/inherit、editor status 颜色/线宽、完整 hash 与六组失效订阅均通过；既有 Theme motion 合同保持通过。

## Linux 后台原生窗口

```sh
cmake --build --preset linux-gcc-debug --target rynui_token_gallery -j2
ctest --preset linux-gcc-debug -R '^rynui\.gallery_native_mentions$' --output-on-failure
```

preset `linux-gcc` / Debug；既有 Gallery runner 经 `tools/run_background_wayland.py` 创建私有 headless Wayland。**1/1 CTest，2 页（Mentions→Button），185 checks，42 submitted frames，0 失败**；Vulkan/SPIR-V、system fonts、实际 host input-area/cursor 同步。实际检查多行 caret popup、原生替换、受控拒绝、Core composition Enter、Form reset、长列表及 input-first wheel、合成异步 latest authority、带 pending/editor capture 离页清理，之后 Button 健康。最终目录状态生成后重复通过相同计数；未回退到桌面。

## 目录与文档

```sh
python tools/update_ant_design_gallery_catalog.py --write --check --self-test
python tools/generate_gallery_source.py --output out/mentions/generated_gallery_source.inc
python tools/generate_gallery_source.py --output out/mentions/generated_gallery_source.inc --check
python tests/gallery_source_tests.py
python tests/verify_mentions_theme_baseline.py
python scripts/format-code.py --check --clang-format /home/jiang/code/RynUI/out/linux-validation/format-tools/bin/clang-format
openspec doctor --json
openspec validate --all --strict --no-interactive
git diff --check
```

结果：原 87 个源码示例保留，新增 1 个为 **88**，源码/API/token 检查 **88/88**；运行时验证冻结 legacy inventory 仍为 **458**。格式化 **933** 个自有源码、0 失败（22.1.3）；doctor healthy，strict **107/107**。只有 Mentions overlay 从 planned 改为 implemented。原始源码副本、日志、JSON、构建与窗口产物只保留在 ignored `out/`。

本结果不声明 Windows/MSVC、物理系统 IME 候选 UI、系统辅助技术、实际跨显示器/DPI 或人工桌面体验通过；Core composition 不替代这些证据。Gallery 异步示例明确为应用合成、显式交付，不访问网络。Web DOM/CSS/React/HTML 与浏览器 portal/transport 不属原生合同。63.2/64.2/65.1 继续由父线程汇总，未在本分支勾选。
