# Form Linux 平台通用验收（13.1 / 13.2）

- 独立基线 `6212a9c5684259c8c810200d3343f7e1f43212ea`，Linux x86_64、GCC 16.2.1、`linux-gcc-headless`、Ninja Multi-Config、BUNDLED、HEADLESS/Recording，构建始终 `-j2`。
- 上游固定 [Ant Design 6.6.5 / Form](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/form/index.en-US.md)，同时对照 Form/Item/List/context/useForm 类型及 basic、dynamic、complex-control、trigger、dependencies demos；副本仅在忽略的 out。
- 最终受影响检查：Debug **46/46**；Release **46/46**。包含 Form 9 项、InputNumber 4 项、AutoComplete 4 项、Select 5 项、Gallery 5 项、portable Input/selection/Theme/Core 边界 19 项。文档三个 C++ 片段实际编译。
- Form 重点覆盖：深/嵌套稳定列表、富草稿/preedit/焦点和删除代际；未挂载 typed store/watch/后续注册/reset；受控拒绝/同值 reset；自动绑定及 custom adapter；实际列/Inline/RTL/反馈几何；异步乱序、owner-thread、依赖/debounce、超时与取消；重入 getter/setter/meta/cancel、工厂自毁、异步-only List 销毁；Enter/TextArea/IME、裁剪 reveal、布尔控件 ref 重绑/错误线程以及完整 Form/List 退役后无关控件健康。
- Gallery 保留基线 80 个源码样例和冻结 **458 IDs**，增加三个独立实际 Form 示例，实际生成 **83**；验证实际输入/精确金额/深路径提交值、列表增删重置和异步过期取消。父线程合并后需按最终源码重新生成总数。

## 必要命令

配置复用本机已锁定 BUNDLED source，未修改依赖锁或网络/代理设置：

```sh
form_sources=/home/jiang/code/RynUI/out/build/linux-gcc-headless/_deps
cmake --preset linux-gcc-headless \
  -DFETCHCONTENT_SOURCE_DIR_RYNUI_FREETYPE="$form_sources/rynui_freetype-src" \
  -DFETCHCONTENT_SOURCE_DIR_RYNUI_HARFBUZZ="$form_sources/rynui_harfbuzz-src" \
  -DFETCHCONTENT_SOURCE_DIR_RYNUI_UTF8PROC="$form_sources/rynui_utf8proc-src" \
  -DFETCHCONTENT_SOURCE_DIR_RYNUI_SHEENBIDI="$form_sources/rynui_sheenbidi-src"
```

Debug 与 Release 使用相同 35 个受影响 target（下列 shell brace expansion 展开为实际 target）：

```sh
form_targets=(
  rynui_form_{model,async,public_api,component,theme,advanced,interaction,lifecycle}_tests
  rynui_input_number_{model,public_api,component,theme}_tests
  rynui_{auto_complete_complete,select_complete,gallery_sample_page,ant_design_reference_catalog,gallery_source_panel}_tests
  rynui_portable_{input_component,input_variant,input_count,input_actions,input_caret_blink,input_public_api,text_area,input_display,focus_lifecycle,selection_component,selection_controls_public_api,switch_features,switch_public_api,checkbox_group,checkbox_public_api,theme_algorithm,theme_runtime,theme_public_api}
)
cmake --build --preset linux-gcc-headless-debug -j2 --target "${form_targets[@]}"
cmake --build --preset linux-gcc-headless-release -j2 --target "${form_targets[@]}"
form_checks='model|async|public_api|component|theme|advanced|interaction|lifecycle|documentation'
portable_checks='input_component|input_variant|input_count|input_actions|input_caret_blink|input_public_api|text_area|input_display|focus_lifecycle|selection_component|selection_controls_public_api|switch_features|switch_public_api|checkbox_group|checkbox_public_api|theme_algorithm|theme_runtime|theme_public_api|backend_boundaries'
checks="^rynui\.(form_($form_checks)|input_number_(model|public_api|component|theme)|auto_complete_complete_.*|select_complete_.*|portable\.($portable_checks)|gallery_sample_page|ant_design_reference_catalog|gallery_source_panel|gallery_sample_source_contract|gallery_sample_source_generated)$"
ctest --preset linux-gcc-headless-debug --output-on-failure -R "$checks"
ctest --preset linux-gcc-headless-release --output-on-failure -R "$checks"
cmake --build --preset linux-gcc-headless-release -j2 --target rynui_ant_design_reference_catalog_tests rynui_gallery_sample_page_tests rynui_gallery_source_panel_tests
ctest --preset linux-gcc-headless-release --output-on-failure -R '^rynui\.(ant_design_reference_catalog|gallery_sample_page|gallery_source_panel|gallery_sample_source_contract|gallery_sample_source_generated)$'
python tools/generate_gallery_source.py --output out/form-work/form-gallery-source.inc
python tools/generate_gallery_source.py --output out/form-work/form-gallery-source.inc --check
python tools/update_ant_design_gallery_catalog.py --write
python tools/update_ant_design_gallery_catalog.py --check --self-test
python scripts/format-code.py --check --clang-format /home/jiang/code/RynUI/out/linux-validation/format-tools/bin/clang-format
openspec doctor --json
openspec validate --all --strict --no-interactive
git diff --check
```

目录状态更新后：Debug 受影响 **46/46**（包含 Gallery/目录/source 五项），Release Gallery/目录/source **5/5**。clang-format **22.1.3，916 文件零失败**；doctor **healthy**；严格校验 **107/107**；diff check 通过。

## 失败、边界和交付

空列表同名叶别名拒绝修复的定向 Debug **3/3** 通过，并纳入最终两配置复验。早期检查暴露未挂载 store、异步-only List 销毁 pending、反馈插槽层级兼容和取消重入边界，已修复并加入/保留回归。测试夹具的取消钩子、Selection 宿主及二次 mount 误用已纠正。一次重复构建导致的缺失对象链接失败已作废，最终以单个串行构建后的结果为准；最终检查无失败。原始日志、下载源码、生成内容及二进制均只在忽略的 out。

本证据为实际 CPU/Recording/HEADLESS 合同，合成异步不代表网络服务；不宣称原生 GPU/窗口、物理系统 IME candidate UI、AT、跨显示器或 Windows/MSVC 通过。未运行桌面输入或截图。63.2/64.2/65.1 保留父线程收口；其他组件的新主线 API/adapters 由父线程按集成顺序合并。本分支仅本地提交，无 upstream、无 push、无 PR/archive。
