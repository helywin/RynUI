# Form 主线集成验收

2026-11-05；基线 `45a11d2ec09b6ab213af7fd56f26a0cb07f5ad28`，按序集成本地 `b9624082`、`b9d477f6`、`643b0ceb`、`19082a82`、`3141a822`、`6cf0c523`。原分支平台通用结果见 [form-linux.md](form-linux.md)。保留 Mentions/Transfer focus observers、InputNumber exact Decimal/formatter/hold/wheel、现有 Select/AutoComplete/DateTime、ref/geometry 与异步取消重入合同。

Linux x86_64 / GCC 16.2.1 / Ninja Multi-Config / BUNDLED / `linux-gcc-headless`：组合 **Debug 70/70、Release 70/70**。覆盖 Form 模型/异步/公开 API/组件/Theme/布局/List/交互/生命周期/文档，InputNumber、selection/Select、AutoComplete、Mentions、Date/Time String reset、Transfer 全部 15 个合同、Gallery、公开头及 Core 边界。三个文档 C++ 片段实际编译。

```sh
cmake --preset linux-gcc-headless
cmake --build --preset linux-gcc-headless-<debug|release> --target <受影响 targets> -j2
ctest --preset linux-gcc-headless-<debug|release> -j2 --output-on-failure -R '<下列正则>'
```

构建 targets：`rynui_form_{model,async,public_api,component,theme,advanced,interaction,lifecycle,integration}_tests`、`rynui_input_number_{model,public_api,component,theme}_tests`、`rynui_{auto_complete_complete,select_complete,gallery_sample_page,ant_design_reference_catalog,gallery_source_panel}_tests`、`rynui_mentions_{tests,theme_tests,public_api_tests,gallery_tests}`、`rynui_{date_picker_review,time_picker_review,transfer_complete}_tests`；portable input_component/input_variant/input_count/input_actions/input_caret_blink/input_public_api/text_area/input_display/focus_lifecycle/selection_component/selection_controls_public_api/switch_features/switch_public_api/checkbox_group/checkbox_public_api/theme_algorithm/theme_runtime/theme_public_api。

```text
^rynui\.(form_(model|async|public_api|component|theme|advanced|interaction|lifecycle|documentation|integration)|input_number_(model|public_api|component|theme)|auto_complete_complete_.*|select_complete_.*|mentions_.*|date_picker_review_form_reset|time_picker_review_reset|transfer_complete|portable\.(input_component|input_variant|input_count|input_actions|input_caret_blink|input_public_api|text_area|input_display|focus_lifecycle|selection_component|selection_controls_public_api|switch_features|switch_public_api|checkbox_group|checkbox_public_api|theme_algorithm|theme_runtime|theme_public_api|backend_boundaries)|gallery_sample_page|ant_design_reference_catalog|gallery_source_panel|gallery_sample_source_contract|gallery_sample_source_generated)$
```

后续原生运行发现同步依赖字段旧 debounce 撤销新异步提交的竞态；新增确定性回归先失败后修复。校验在发布 meta 前消费其快照覆盖的旧 deadline，保留 validate-only 与取消重入语义。该修复后最终定向 **Debug 12/12、Release 12/12**、失败 **0**；沿用上述未受影响基线，未运行全量 CTest。构建 Form model/async/component/advanced/interaction/lifecycle/integration、Gallery sample page、Mentions、AutoComplete complete、Date/Time review targets，均 `-j2`；运行：

```text
ctest --preset linux-gcc-headless-<debug|release> -j2 --output-on-failure -R '^rynui\.(form_(model|async|component|advanced|interaction|lifecycle|integration)|gallery_sample_page|mentions_form|auto_complete_complete_form_editor|date_picker_review_form_reset|time_picker_review_reset)$'
```

集成回归还捕获并修复私有编辑器/选择控件抢占外层 FormItem 绑定或 Enter、富 popup 串入外层字段、Transfer 私有 input-block 退休残留，以及清理触发的旧行表重入。按窗口宿主/Item 隔离私有控件，保留嵌套 FormItem；Transfer 先发布完整新行/key 权威再删除旧行，私有分支各自清理 gates。新回归覆盖富 popup 独立值/Enter、显式 MentionsRef 同值 reset、Date/Time/Cascader/Transfer/ColorPicker 后的公开 Input、数据收缩与局部退休后继续编辑。Transfer 原有 **15/15 合同**保留通过。测试夹具补齐 Selection host；原生断言纠正了 placeholder 与编辑值、稳定 key 起点的区别；生成元数据已刷新。没有删除功能或放宽合同断言。

`cmake --build --preset linux-gcc-release --target rynui_token_gallery -j2`；`ctest --preset linux-gcc-release -R '^rynui\.gallery_native_form$' --output-on-failure`：**1/1 CTest、4 页、976 checks、213 submitted frames、0 失败**。Release Gallery 经 `run_gallery_page_acceptance.py`→`run_background_wayland.py`，实际 Form→Mentions→DatePicker→Button；Vulkan/SPIR-V/system fonts、host input area/cursor。实际布局/RTL、滚动后聚焦、自动与 custom typed adapter、超 2^53 精确金额、深列表增删移动/草稿/实际提交路径、同步/异步 Enter、multiline/preedit/reset、latest/cancel/submit 与 pending/preedit/capture 同时存在时退休均通过，后继控件健康。保留 frame callback、有限 pacing、正常 SDL pump，无桌面 fallback。

全部五份完整 **62 顶层字段** Theme golden、Theme API/runtime 与 Mentions 提交字节一致；冻结 **458 IDs** inventory 源文件与 `a0e7c7e` 字节一致。目录 `--check --self-test`、source generator 生成/`--check` 与 source/API/token 检查通过，实际 **91** 个样例，保留其他 extractors。clang-format **22.1.3** / `scripts/format-code.py --check` **950 文件、0 失败**；doctor healthy、strict **107/107**、`git diff --check` 通过。原始日志、JSON、图片、编译探针和二进制只在 ignored `out/`。

HEADLESS/Recording 不作为原生 GPU 证明；合成异步/Core composition 不证明网络服务或物理系统 IME。Windows/MSVC、系统 IME candidate UI、AT、实际跨显示器/DPI、人工桌面未验证。仅关闭 13.1/13.2；63.2/63.3、64.*、65.1 保持开放。Release Gallery 已重建，未启动到用户桌面。
