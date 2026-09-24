# 窗口编辑设施阶段验收

- 日期：2026-09-25；平台与工具链沿用窗口服务阶段的 Windows 11 / MSVC x64；正式构建使用 `windows-msvc-debug` 与 `Ninja Multi-Config`。
- `WindowTextEditServices` 由窗口服务按需持有 `TextEditorStore`、`TextInputSessionHost` 和 `TextClipboardCommands`。Input 持有受限引用；相同端口重复绑定返回同一对象，冲突端口在创建第二个平台会话前抛出 `std::logic_error`。Input-only 窗口测试覆盖绑定、会话、销毁及端口冲突。
- `./scripts/build-windows.ps1 -Configuration Debug -SkipTests` 构建通过。
- `ctest --preset windows-msvc-debug -R "^rynui\.(input_(component|pointer|keyboard|journey|gpu|scene_allocation)|button_component|token_gallery_frame)$" --output-on-failure`：8/8 通过；`input_scene_allocation` 112.19 秒通过，覆盖稳定场景的分配/闲置合同。
- 不改变编辑算法、公开 Input API 或平台端口实现；本阶段不声称完成真实系统 IME 人工验收。
