# 065 平台通用验收

2026-10-23，Windows x64 / MSVC 19.51.36256 / C++20，`windows-msvc-headless`，Ninja Multi-Config，最多 4 个编译任务。独立构建目录为本工作树 `out/build/windows-msvc-headless`，未复用其他工作树的 build。

实现提交：`56317a96aeeec3f17d6680f129c29652b65c5542`。
依赖为 Tag/Badge 原提交 `777939d3149ea679ade6668a4c45998398667fde`，在指定父提交 `4e258e2afe832323c934e4ebfc3fb2144fc1608f` 上 fast-forward；不重复列为本任务新增提交。未引入 Drawer 分支。

## 实际结果

| 验证 | Debug | Release |
|---|---|---|
| 定向 CTest | 15/15，4.41 s | 15/15，3.75 s |
| feedback public API / Theme / component | 通过 | 通过 |
| Tag/Badge public API / Theme / component | 通过 | 通过 |
| portable Icon / Modal boundary / Modal / Input | 通过 | 通过 |
| portable Theme algorithm / Theme runtime | 通过 | 通过 |
| portable logical scene packing / Recording scene backend / backend boundaries | 通过 | 通过 |

新测试实际覆盖受控 pointer/Space close、独立 action、slot 身份、颜色不测量、size token 更新、零/窄约束、invalid mount 与 close 回调销毁；Spin 内容恢复、嵌套和动态子节点、capture/编辑会话清理、blur 重入销毁、抛异常 indicator 清理、reduced/window inactive/offscreen 停止和恢复动画；Progress clamp/invalid 值、实际 CPU rounded fill 的 line 比例、circle 象限、dashboard 缺口及稳定 topology。圆形 rail 使用单个 outline，避免半透明剩余色的重复叠加。

## 复现入口

在 Visual Studio Developer Environment 中执行 `cmake --preset windows-msvc-headless`。分别使用 `windows-msvc-headless-debug` 和 `windows-msvc-headless-release` build/test preset，build 参数为 `--parallel 4`，仅构建下列 test targets：

```text
rynui_feedback_public_api_tests rynui_feedback_theme_tests rynui_feedback_component_tests
rynui_tag_badge_public_api_tests rynui_tag_badge_theme_tests rynui_tag_badge_component_tests
rynui_portable_icon_component rynui_portable_modal_boundary rynui_portable_modal_component
rynui_portable_input_component rynui_portable_theme_algorithm rynui_portable_theme_runtime
rynui_portable_logical_scene_packing rynui_portable_scene_backend
```

两种 configuration 的 CTest 均使用 `--parallel 4 -R` 和以下表达式，没有重复运行整套 CTest：

```text
^rynui\.(feedback_.*|tag_badge_.*|portable\.(icon_component|modal_boundary|modal_component|input_component|theme_algorithm|theme_runtime|logical_scene_packing|scene_backend|backend_boundaries))$
```

## 来源与文档检查

- 锁定 Ant Design commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 style source 已读取并核对 SHA256：Alert `7a2a9e8129ed59f25b1cdfb40a4b3bcb3d24e2fbf33be04707bd5cf7b7e0bd72`；Spin `375ca644daf9026c65dc19c3163f30ca04467c6a8ddf63d4fca08aa4cc74b962`；Progress `fef30dca7e6c22102b7337728f2c764df73a6f5f1ec41719d79e106c10929ce0`。均匹配 sources.lock.yaml。
- 五份 golden 由当前 `rynui_portable_theme_algorithm --write-goldens` 生成，逐份 JSON 对比依赖提交：旧字段全部相等（整体 identity 除外），新增字段恰为 alert/spin/progress；随后 Debug/Release golden 测试均通过。
- clang-format 22.1.3；`scripts/format-code.py --check` 检查 514 个自有源码，0 failures。
- `openspec doctor --json` healthy；`openspec validate --all --strict --no-interactive` 63/63；`git diff --check` 通过。
- 新组件未修改 renderer 源码、CPU scene v3/GPU ABI v2；实际 HEADLESS/Recording 构建与 backend boundary 测试通过。现存 Button/Text/Typography/Input 测试 warning 未在本任务修改。

## 明确边界

API、token、支持缺口见 [feedback.md](../../../docs/feedback.md)（仓库根目录 docs/feedback.md）。此处仅记录平台通用 headless 证据，没有原生 GPU、真实窗口/DPI、Linux、系统 IME 或人工视觉验收。圆弧为 192 点原生近似；dashboard rail 端部裁剪和半透明前景交叠限制已公开记录。

Gallery、README、全局 support overlay 未修改。没有创建 PR、archive 或子代理。用户在规划提交已推送后更新策略；之后仅本地提交，不再 push、重建远端 t3 分支或变更其他工作树，父任务负责最终合并、验证和 main 推送。
