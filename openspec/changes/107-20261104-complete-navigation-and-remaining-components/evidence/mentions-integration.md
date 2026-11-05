# Mentions 主线集成验收

2026-11-05；main 基线 `a0e7c7ed858385e6d4469c20d60719775a8b28ba`，依次集成本地 `e3cec5aa`、`4dd6680c`。原分支合同见 [mentions-linux.md](mentions-linux.md)。保留 Transfer/Pagination/SDL ordered modifier、InputNumber exact Decimal/formatter/hold/wheel、AutoComplete TextArea、全部 focus observers 与 private-region validation。

- Linux x86_64，GCC 16.2.1；CMake presets / Ninja Multi-Config / BUNDLED。`linux-gcc-headless` Debug **44/44**、Release **44/44**，最终失败 **0**。
- 构建 `rynui_mentions_{tests,theme_tests,public_api_tests,gallery_tests}`、`rynui_theme_{algorithm,runtime}_tests`、`rynui_auto_complete_complete_tests`、`rynui_input_number_component_tests`、`rynui_transfer_{complete,theme}_tests`、`rynui_form_component_tests`；portable input_component/text_area/input_actions/input_count/input_variant/input_public_api/theme_scope_component/focus_lifecycle 及 Gallery sample_page/reference_catalog/source_panel targets；均 `-j2`。
- 必要命令：`cmake --preset linux-gcc-headless`；`cmake --build --preset linux-gcc-headless-{debug,release} --target <上述 targets> -j2`；`ctest --preset linux-gcc-headless-{debug,release} -j2 --output-on-failure -R <下列正则>`。

```text
^rynui\.(mentions_|auto_complete_complete_|input_number_component$|transfer_(complete|theme)$|form_component$|theme_(algorithm|runtime)$|portable\.(input_component|text_area|input_actions|input_count|input_variant|input_public_api|theme_scope_component|focus_lifecycle|backend_boundaries)$|ant_design_reference_catalog$|gallery_(sample_page|source_panel|sample_source_contract|sample_source_generated)$)
```

`out/build/linux-gcc-headless/tests/Debug/rynui_theme_algorithm_tests --write-goldens` 重新生成五份完整 golden；`python tests/verify_mentions_theme_baseline.py` 比较实际 main 的 **61 个旧顶层字段**，除整体 identity 外完全一致，含 Transfer；新增 Mentions 后共 **62**。旧 TokenIdentity 名称/顺序不变。

`cmake --preset linux-gcc`；`cmake --build --preset linux-gcc-release --target rynui_token_gallery -j2`；`ctest --preset linux-gcc-release -R '^rynui\.gallery_native_mentions$' --output-on-failure`：**1/1 CTest、4 页、590 checks、131 submitted frames、0 失败**。实际 Release Gallery 经 `run_gallery_page_acceptance.py` → `run_background_wayland.py`，Mentions→AutoComplete→Transfer→Button；Vulkan/SPIR-V/system fonts，实际 host input-area/cursor。覆盖 caret/query/native replacement、受控拒绝、Core preedit Enter、同值 reset、保留富 Input 草稿、长列表 input-first wheel、合成异步 latest、pending/editor/capture/session 离页清理及后继控件健康。保留 frame callback/有限 pacing/SDL pump，无桌面回退。

目录 `--write --check --self-test`、源码 generator 生成/`--check`、`tests/gallery_source_tests.py` 全通过，实际 **88** 样例；冻结 **458 IDs** 对应 inventory 源文件与起始 main 字节一致。clang-format **22.1.3** / `scripts/format-code.py --check` **938 文件、0 失败**；doctor healthy，strict **107/107**，`git diff --check` 通过。

早期仅有预期的 Theme/测试注册/生成目录文本冲突，已追加合并；最终构建与验证无失败。原始 logs/JSON/images/二进制只在 ignored `out/`。HEADLESS/Recording 不作原生证明；合成异步与 Core composition 不证明网络服务或物理 IME。Windows/MSVC、系统 IME candidate UI、AT、实际跨显示器/DPI、人工桌面体验未验证。仅关闭 16.1/16.2；63.2/64.2/65.1 保持开放。Release Gallery 已构建，未在用户桌面启动。
