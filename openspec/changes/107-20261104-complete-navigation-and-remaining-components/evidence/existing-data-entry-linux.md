# 现有 DataEntry 六组控件收口

基线：`45a11d2e`；仅 10、14、17、18、20、21 的实现与 Gallery 子任务。本地 T3 分支交付，父线程负责合入与推送。63.2、64.2、65.1 保持父任务状态。

## 合同核对与实际修改

对照固定 [Ant Design 6.6.5 源码](https://github.com/ant-design/ant-design/tree/4a39f54842eade4e565ab336ef6097cd7e723cdd/components)（`4a39f54842eade4e565ab336ef6097cd7e723cdd`）的 `components/{checkbox,input,radio,rate,slider,switch}` API、类型、实际 demos 与样式；源码副本只在忽略的 out。设计矩阵中的历史缺口与源码逐项核对，未以旧测试通过替代本次审查。

| 组件 | 本轮实际原生缺口及修改 | 沿用且未重写的合同 |
| --- | --- | --- |
| Checkbox | onFocus/onBlur 实际焦点转换回调，回调副本与同步销毁安全；Gallery 显示回调 | 047 的 String/double/bool、Group/最近组/skipGroup、动态 options/清理、authority/default、富标签、RTL/ref/autoFocus、独立 Theme、有限 wave；历史共同及 Windows 结果见 047 evidence/windows/README.md |
| Radio | 补齐可原生表达的焦点回调，保留现有 roving 与按钮合同 | 048 的三类值、最近 typed Group、动态保留选项、受控请求、单一 Tab/方向键、富标签、H/V/RTL、outline/solid/block/三尺寸、ref 与有限 wave；见 048 evidence/common/README.md 及 056 补充证据 |
| Switch | 补齐焦点进入/离开回调，原生样例显示实际事件 | 046/056 保留的两分支内容、默认/受控 authority、disabled/loading、RTL/ref/autoFocus、阴影/按压连续性与有限 wave；未重跑历史像素矩阵 |
| Slider / RangeSlider / MultiSlider | 锁定类型中的 onFocus/onBlur 转换事件；原生样例覆盖受控 keyboard 回调 | 039/041/042 的 limits/精度、marks/dots/Tooltip、范围拖动、动态编辑、逐端点门禁、reverse/vertical、ref 与捕获生命周期；未复制历史矩阵 |
| Rate | 新增显式响应 RTL、RateRef/autoFocus、onFocus/onBlur/onKeyDown、逐项 Prop 纯绘制 character；RTL 同步排列、半星裁剪、pointer/keyboard，方向变化取消旧捕获；动态 character 初次绑定失败回滚新增项 | 090/093 的权威值、半星/清空、动态 count、单一 Tab、尺寸/Theme、富 Tooltip 单次挂载、readOnly 后代、零尺寸/输入边界、有限离开宽限；既有共同结果见 090 evidence/main-integration.md |
| Input / Search / Password / TextArea / OTP | 本轮未改编辑器实现；canonical 五个 family route 接入实际原生输入/离页清理动作，修正文档中已过时的 bidi 待实现描述 | 052–055、010/014/020/022 的 Unicode/bidi/grapheme、clipboard/history/IME bridge、计数/formatter、caret/ref、Password、Search、TextArea autoSize/resize/wheel、OTP mask/formatter/动态长度；保留 InputNumber、Mentions 和 AutoComplete TextArea 的集成合同 |

新增 `existing_data_entry_completion` 的七组实际合同覆盖 RTL 真实 glyph 裁剪与键盘/指针、逐项响应身份/隐藏恢复、增长异常回滚、ref 线程/重复绑定/复用、事件异常与同步销毁、抛出 blur 后仍完成捕获/焦点/场景/ref 退役、selection 实际焦点转换、Slider 逐端点门禁/焦点转移。没有新增组件私有上传、GPU ABI、renderer、SDL 或系统字体依赖；Theme/goldens 没有修改。

## 验证

Linux GCC / Ninja Multi-Config / BUNDLED，`linux-gcc-headless`，构建并行度 2。锁定依赖源从 main 的 out 缓存复制到本工作树 `out/dependency-sources` 后，用 `FETCHCONTENT_SOURCE_DIR_*` 覆盖 preset；未修改 lock。下载源码与现有 source-contract fixture 的 7 个文件哈希相符。

同一组 **36 个不同 CTest** 的结果：Debug 初轮 34/35（新增异常退役用例暴露 pointer cancel 会先同步取消焦点，修正顺序并确保资源清理后通过）；只重跑受影响的 7 项及 backend boundary，**8/8**。Release 初轮 **35/36**，唯一失败为新 Gallery target 添加后的生成源码元数据过期，实际重生成/重建后该项 **1/1**；实现和集成项全部通过。未重跑已通过的旧全矩阵。新增 completion 可执行文件报告 **7 组合同**通过。

构建命令使用 `cmake --build --preset linux-gcc-headless-debug` 与 `linux-gcc-headless-release`，两者均加 `--parallel 2 --target`，target 为下列 27 项：

```text
rynui_auto_complete_complete_tests
rynui_bidi_input_tests
rynui_existing_data_entry_completion_tests
rynui_gallery_sample_page_tests
rynui_input_number_component_tests
rynui_input_number_model_tests
rynui_mentions_tests
rynui_portable_checkbox_group
rynui_portable_input_actions
rynui_portable_input_component
rynui_portable_input_count
rynui_portable_otp_interaction
rynui_portable_password_component
rynui_portable_radio_features
rynui_portable_search_component
rynui_portable_selection_component
rynui_portable_slider_component
rynui_portable_switch_features
rynui_portable_text_area
rynui_portable_theme_algorithm
rynui_portable_theme_runtime
rynui_rate_component_tests
rynui_rate_focus_order_tests
rynui_rate_lifecycle_tests
rynui_rate_public_api_tests
rynui_rate_theme_tests
rynui_token_gallery_frame_tests

```

CTest 命令为 `ctest --preset linux-gcc-headless-{debug,release} --parallel 2 --output-on-failure -R '<filter>'`，完整筛选如下（Debug 初轮不含末尾 backend_boundaries；修正回归限定 completion、rate_component/lifecycle/focus_order、portable.selection_component/switch_features/radio_features/backend_boundaries；生成源码复验只筛选 gallery_sample_source_generated）：

```text
^rynui\.(existing_data_entry_completion|rate_(component|lifecycle|public_api|theme|focus_order)|bidi_input|input_number_(model|component)|mentions_(caret|composition|form|theme_baseline)|auto_complete_complete_form_editor|gallery_(page_input_frame|page_members-metrics_frame|sample_page|sample_source_contract|sample_source_generated)|ant_design_gallery_catalog_(generator|contract)|portable\.(selection_component|switch_features|checkbox_group|radio_features|slider_component|input_component|input_actions|input_count|text_area|otp_interaction|password_component|search_component|theme_algorithm|theme_runtime|backend_boundaries))$
```

五份 Theme goldens 原样保持 **62 个顶层字段（含 Mentions）**，Theme algorithm/runtime 与 Mentions baseline 通过。clang-format 22.1.3 检查 **941 文件、0 failures**；OpenSpec doctor healthy，strict all **107/107**，diff check 通过。

原生 `linux-gcc` Release / GNU / BUNDLED / Ninja Multi-Config，最多 2 jobs：

```text
cmake --build --preset linux-gcc-release --parallel 2 --target rynui_token_gallery
python tools/run_gallery_page_acceptance.py out/build/linux-gcc/examples/Release/rynui_token_gallery --log-dir out/change107-existing/native --preset linux-gcc --configuration Release --timeout 120 --page checkbox --page radio --page switch --page slider --page rate --page input/input --page input/search --page input/password --page input/text-area --page input/otp --page button
```

只通过 `run_gallery_page_acceptance.py → run_background_wayland.py` 的私有 headless Mutter/Wayland/DBus 执行，无桌面回退；保留 compositor frame callback、事件轮询和刷新率 pacing。实际 **11/11 页、585 checks、110 submitted frames**；backend **Vulkan**、shader **SPIR-V**、font_source **system**、system_fonts/host_input_area/host_cursor_sync 均为 true。一次有限序列按原 host 的各页宽度/主题运行，没有重复整个主题×尺寸/像素矩阵。

| 原生页 | 实际动作与结果 |
| --- | --- |
| Checkbox | typed number 受控回写、Space、disabled、动态选项 2→3 身份保留、ref/focus 事件；捕获后离页释放 |
| Radio | 受控 Group 键盘回写、disabled、roving、动态 identity、solid typed 选择、ref/focus；捕获后离页释放 |
| Switch | pointer/Space 受控回写、focus、disabled/loading 门禁；捕获后离页释放 |
| Slider | 受控键盘/focus、实际整段拖动保持间距、MultiSlider Delete、逐端点门禁；捕获后离页释放 |
| Rate | 半星/受控拒绝、RTL 与逐项字符更新保持身份、ref/keydown、disabled/readOnly；富 Tooltip 实际 Input 草稿经过隐藏恢复保留，捕获后离页释放 |
| Input / Search | 实际 committed Unicode 与提交回调；带 capture/preedit/session 离页清理 |
| Password | 实际源文编辑与 mask 隐藏；带 capture/preedit/session 离页清理 |
| TextArea | Unicode/多行、autoSize 增高；带 capture/preedit/session 离页清理 |
| OTP | 实际 formatter 大写/六格分发/mask、整格点击选择、左右键导航；preedit/session 离页清理。OTP 原合同不建立文本拖选捕获，没有为验收添加私有 capture |

原生初轮在动作脚本发现两类错误假设：SliderSnapshot 中心点需加滚动 translation；OTP 点击按原合同整格选择，不建立普通文本拖选 capture。修正的是验收坐标/断言，未改变 Slider/Input 默认行为；最终完整序列为上述通过结果。

实际源码生成器 **88 sources**，冻结 legacy IDs/targets **458/458**；没有增删既有样例或冻结 ID。Catalog generator write/check/self-test、source generator/check/self-test 通过；仅修改六个自有 overlay entries，Rate 从 partial 提升为已验证的原生范围，C++ 纯绘制映射限制保持公开。

本轮关闭 10.1/10.2、14.1/14.2、17.1/17.2、18.1/18.2、20.1/20.2、21.1/21.2。**63.2、64.2、65.1 仍未勾选**；其他工作树 Form/Tree/Upload 等最终组合由父线程集成，未以此有限序列冒充整个 DataEntry 分类验收。

## 原生映射与边界

- RateCharacter 是可复制的 String/IconSource 绘制值，支持两份裁剪绘制；characterFor 逐项绑定 Prop，并非有状态 Component/ReactNode 的两份克隆。富 Tooltip 继续通过一个原生标题 slot 保存 Input 草稿/ref，Rate 管理 hover/focus 开合和宽限；不承诺任意 Web TooltipProps/DOM 事件透传。
- 原生 onFocus/onBlur 与 RateKeyEvent 不输出 React/DOM/SDL 事件对象；LayoutStyle 只影响外部布局，视觉仍由完整现有 Theme 提供。ConfigProvider 全局上下文为独立 Other 能力，本轮显式属性不代表其实现。
- DOM/HTML/CSS、name/required、autofill、portal、React ref/事件、废弃别名（包括 Switch value/defaultValue）排除在本轮原生合同之外。
- HEADLESS/Recording 和合成 composition 不代表物理 IME 候选 UI、OS 辅助技术、Windows MSVC、实际跨显示器或人工视觉验收。历史通过平台记录保持；本轮只新增实际运行的 Linux 定向窗口/GPU/输入证据，不重复旧主题/尺寸/像素组合矩阵。原始日志、JSON、图像及二进制只在 out。
