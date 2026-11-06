# Avatar Linux 定向验收

这是隔离来源提交的验证记录；当前 main 的重新验证见 [Avatar/Badge/Tag 集成](avatar-badge-tag-integration.md)。
2026-11-05；隔离源分支 `t3/implement-avatar-badge-tag`，基线 `135ba70b`。固定 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd`，核对 public API/types/styles/demos/tests；上游副本、原始运行产物仅在 ignored out。

- Linux x64 / GCC 16.2.1 / BUNDLED / Ninja Multi-Config，configure preset `linux-gcc-headless`，build presets `linux-gcc-headless-debug`、`linux-gcc-headless-release`，最多 `-j2`。最终受影响集合共 **57 个 CTest 项**：Debug **57/57**；Release 首轮 **56 passed + 1 Not Run**（遗漏目录 executable 的 build target），补建后仅重跑该项 **1/1**，最终 57 项全部通过。目录刷新及 Tag 增补测试后 Debug 定向 **7/7**。
- Avatar native 合同在 Debug/Release 各 **37 条断言**，与 CTest 项数分开计数。覆盖实际四色 RGBA、circle/rounded-square 公共形状、错误/回退/重试、onError(false)、过期/重复/外线程 completion、cancel 重入和 callback throw 退役；真实窗口逻辑断点、窄/零空间恢复、稳定键 add/remove/reorder、继承/RTL、保留溢出 Input 与开合通知。旧 Avatar text/group/overflow/clip/retention/gates/lifecycle/validation 均保留通过。
- 共同集合包括 Image component **19 项**、Image resources **7 项**、旧 Tag/Badge API/Theme、Input/focus/pointer、Form integration/lifecycle、Core include/link guard、Gallery/source/catalog/draft codec 和 Theme algorithm。新旧公开 API **3 个 targets** 在 D/R 构建通过：`rynui_image_public_api_compile`、`rynui_tag_badge_public_api_tests`、`rynui_avatar_badge_tag_public_api_compile`；新例只使用实际 `Signal`/typed Props/slots。
- 三个独立实际 Gallery 的 Recording CTest **3/3**；串行 `rynui_status_gallery_tests all` 在 D/R 各 **38 条检查**。Avatar 页实际完成/失败/取消重试、真实 pixels、响应分组/溢出及稳定键重排，不是 mount-only。
- writer/source generator 与源码合同实测 **96 samples = 原 93 + 3**，**458 frozen legacy IDs**。五份完整 Theme golden 各 **64 顶层字段**，含 Image/Upload，逐字节不变于 `135ba70b`；组件继续使用 owned Avatar/Badge/Tag Token，不改旧 TokenIdentity/ordinal/aggregate order。

共同受影响测试选择器（实际用于 Debug/Release）：

```text
ctest --test-dir out/build/linux-gcc-headless -C Debug --output-on-failure -R '^rynui\.(avatar_.*|badge_native_.*|tag_native_.*|tag_badge_.*|image_component_.*|image_resource.*|form_integration|form_lifecycle.*|theme_algorithm|ant_design_reference_catalog|gallery_source_panel|gallery_sample_page|gallery_sample_source_.*|gallery_draft_codec|portable\.(input_component|focus_lifecycle|pointer_route|backend_boundaries))$'
cmake --build --preset linux-gcc-headless-release --target rynui_ant_design_reference_catalog_tests -j2
ctest --test-dir out/build/linux-gcc-headless -C Release --rerun-failed --output-on-failure
```

相关构建 targets 为 `rynui_avatar_native_contract_tests`、`rynui_avatar_component_tests`、`rynui_avatar_theme_tests`、`rynui_status_gallery_tests` 及上述共同 API/Image/Form/Input/Theme/Gallery targets；均使用两个 HEADLESS build preset 与 `-j2`。目录 writer 的 write/check/self-test、source generator 的 write/check、format-code（clang-format 22.1.3）、OpenSpec doctor/strict 与 diff check 在收口执行。

共享 Image 基线引用为 **`0cd4817b`**，只涉及 `include/ryn/image.hpp`、`src/component/image_component.cpp`、`docs/components/image.md`，已冻结并交共同 Image worker 语义接管。Avatar 只依赖追加的 `RoundedRectangle`、ImageTheme 圆角和 lazy preview 合同；不依赖拼片数/私有纹理/新 ABI。该旧提交不是最终 GPU 实现，**不得再原样导入 main 覆盖 paint_opacity**。最终 disjoint bounds/source_uv/texture lease 与 GPU 由共同 worker 独立交接。

父集成共同触点：`src/runtime/component_host.{hpp,cpp}` 新增 retained state lease 与原子 direct-child reorder；`src/component/retained_slot.hpp` 复用活动 build context/主题范围做局部挂载；`examples/token_gallery/gallery_draft_codec.hpp` 追加 Responsive<float> 编解码，旧 AvatarSizeValue variant ordinal 不变。`tests/CMakeLists.txt`、`cmake/RynUIGalleryTests.cmake`、Gallery definition/registry/source 和生成 catalog 均需 additive union；Tag/Badge 共用 host 由本分支完整拥有。没有新增全局 opacity helper、decoder、uploader、renderer 依赖或 packed ABI。

本分支 **0 项真实 GPU/原生窗口验收**；HEADLESS/Recording 不代表 Vulkan、Windows/MSVC/D3D12、物理 IME/AT 或实际跨显示器通过。63.3/64.3/65.1 由父线程独立汇总。父集成必须以 generator 合并主线 **97 samples/66 字段**及其它新域，不能用此源分支的 96/64 覆盖。
