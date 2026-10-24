# 平台通用验收

基线：`580a8c3d235c22eb95b11688b509ee1e2ea65e44`。本独立 worktree 仅实现 083；无 children、远端操作、PR、archive、README/Gallery/support overlay/global completion 修改。未进行父线程安排的独立审查，不声明 review CLEAN。

## API/模型阶段

- Windows / MSVC 19.51.36256.0 / Ninja Multi-Config；preset `windows-msvc-headless`；max 4 jobs。
- configure：`cmake --preset windows-msvc-headless -DPython3_EXECUTABLE=C:/Users/jiang/AppData/Local/Programs/Python/Python310/python.exe`。
- Debug/Release：`cmake --build --preset windows-msvc-headless-{debug,release} --target rynui_skeleton_public_api_tests rynui_skeleton_model_tests -j 4`。
- Debug/Release：`ctest --preset windows-msvc-headless-{debug,release} -R '^rynui\.skeleton_(public_api|model)$' --output-on-failure`，各 2/2 通过。
- `python scripts/format-code.py --check --clang-format <clang-format-22-path>`：clang-format 22.1.3、652 个受管源码、0 failures。
- 文档示例编译时发现架构示例中的 `ryn::state` 不是当前公开入口，已使用实际 `ryn::Signal<bool>`；无未解决编译错误。

## Theme/host/scene 阶段

- 新增独立 Skeleton host，真实 logical quads、祖先 clip 时的共同 logical fill、retained content 门禁与无内容退出布局流。
- Debug focused `skeleton_theme|skeleton_component` 各通过；三 Theme、短/零/恢复、精确颜色 dirty、组件/editor 身份、slot 次数和静态 idle 均由实际 fixture 检查。
- Debug/Release build：`cmake --build --preset windows-msvc-headless-{debug,release} --target rynui_skeleton_theme_tests rynui_skeleton_component_tests rynui_portable_theme_algorithm -j 4`；CTest：`ctest --preset windows-msvc-headless-{debug,release} -R '^rynui\.(skeleton_(theme|component)|portable.theme_algorithm)$' --output-on-failure`。Debug 分别运行 2+1 项、Release 3/3，均通过。HEADLESS 的旧 Theme target 实际名称是 `rynui_portable_theme_algorithm`（没有 `_tests` 后缀）。
- fresh 五份 goldens 由新建的 `out/build/windows-msvc-headless/tests/portable/Debug/rynui_portable_theme_algorithm.exe --write-goldens` 生成，保留于 `design-tokens/ant-design/6.6.5/golden/`。
- `python openspec/changes/083-20261025-add-native-skeleton/evidence/verify-baseline.py`：15,047 个旧 scalar typed values 深比较一致，31 个旧 IIFE 原文及顺序一致；只追加 Skeleton 字段及更新 identity。该脚本绑定本 change 的父 SHA，作为本次复核工具，不加入未来全局 CI 门禁。
- fresh SHA256：default `b5ba37131a6216bdff24344607149fd1e8f2bd5dfa6213b59106b9e2d3975af2`；dark `5eb8292734f3d04c0f59ef7ac29a32ef887692d7da768e5b22896fcf0f9265a3`；compact `746354112ee4e8f57f94aa6b93e4843f3caedb651fac756f036fd51b60ec0c68`；dark-compact `8a6867c821251b4c1d7574baec37e67f1ba1ccb9f826fa215c255fdc2a99be5a`；compact-dark `f4416d48d3686748f5338e1ac4fe537c093f4e9a486c4a4317505e2ab7da6eef`。

## 覆盖边界

以上为 Windows 正式 MSVC HEADLESS 上的平台通用合同证据，不代表 GPU/真实窗口/Linux/系统 IME/AT/Gallery 通过。首版不含 active/shimmer、Image/Node 或网络加载。
