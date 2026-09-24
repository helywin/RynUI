# 窗口服务阶段验收

- 日期：2026-09-25；平台：Windows 11 专业工作站版 10.0.26200；工具链：Visual Studio 2026 MSVC 19.51.36256.0 x64；正式 preset：`windows-msvc-debug`，`Ninja Multi-Config`。
- `./scripts/build-windows.ps1 -Configuration Debug`：构建通过，CTest 211/211 通过；其中 `rynui.input_scene_allocation` 通过（113.74 秒）、`rynui.button_spinner_benchmark` 通过、`rynui.token_gallery_frame` 通过。运行记录在本机 `out/013-debug-validation.log`，该忽略目录日志不作为可移植证据。
- 增补 Input-only（没有 Button 宿主）挂载、同步、焦点、单一编辑会话和销毁测试后，再运行 `ctest --preset windows-msvc-debug -R "^rynui\.(input_component|input_journey|button_component|token_gallery_frame)$" --output-on-failure`：4/4 通过。
- `WindowComponentServices` 单独拥有 Text、interaction、hit-test、scene、surface、focus、pointer 和 animation；Button/Input 通过同一窗口服务注册挂载及同步参与者。Gallery 和 Input fixture 显式使用窗口所有权。公开 API、Token 和组件视觉合同未修改。
- `git diff --check` 通过。此阶段只证明内部提取及现有自动回归；编辑设施所有权、按压行为和完整平台验收仍在后续任务。
