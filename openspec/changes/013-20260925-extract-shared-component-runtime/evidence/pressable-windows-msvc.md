# 按压行为阶段验收

- 日期：2026-09-25；Windows 11 10.0.26200、MSVC x64、`windows-msvc-debug` 正式 preset。
- 内部 `PressableBehavior` 仅处理 primary pointer identity、捕获、取消、命中释放与一次性 activation intent。独立状态机测试覆盖禁用/捕获失败、第二指针、旧 interaction generation、移出释放、重复释放和取消；Button 现有交互测试覆盖窗口失焦、loading、Enter/Space、回调自毁及回调前的 pressed/capture 收口。
- `./scripts/build-windows.ps1 -Configuration Debug -SkipTests` 构建通过。
- `ctest --preset windows-msvc-debug -R "^rynui\.(button_component|input_(component|pointer|keyboard|journey)|button_spinner_benchmark|token_gallery_frame)$" --output-on-failure`：7/7 通过；其中 Button spinner benchmark 与 Gallery retained frame 测试通过。此前编辑设施阶段的 `input_scene_allocation` 也已通过，但本项不把旧结果称为本次重跑。
- 新增独立 `rynui.pressable_behavior_allocation`：10 万次按下/命中释放均只产生一次 activation intent，跟踪到 0 次堆分配；与 `rynui.button_component` 同跑 2/2 通过。
- Button 的键盘、loading、hover、focus 和 Token/scene 视觉逻辑仍由 Button 自行决定；Pressable 不调用组件回调或修改 checked 状态。
