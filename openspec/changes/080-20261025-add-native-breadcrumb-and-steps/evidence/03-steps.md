# Steps 实现验收

Windows / MSVC 19.51 / Ninja Multi-Config / windows-msvc-headless（HEADLESS/RECORDING）。build 并发上限 4。

```text
cmake --build --preset windows-msvc-headless-debug --target rynui_steps_component_tests rynui_steps_theme_tests rynui_breadcrumb_component_tests rynui_breadcrumb_theme_tests rynui_navigation_model_tests rynui_portable_theme_algorithm --parallel 4
ctest --preset windows-msvc-headless-debug -R '^rynui.(steps_|breadcrumb_|navigation_model|portable.theme_algorithm)' --output-on-failure
cmake --build --preset windows-msvc-headless-release --target rynui_steps_component_tests rynui_steps_theme_tests rynui_breadcrumb_component_tests rynui_breadcrumb_theme_tests rynui_navigation_model_tests rynui_portable_theme_algorithm --parallel 4
ctest --preset windows-msvc-headless-release -R '^rynui.(steps_|breadcrumb_|navigation_model|portable.theme_algorithm)' --output-on-failure
```

Debug/Release 各 6/6。最后新增焦点/capture 隐藏用例后，以相应两个 preset 单独重建 `rynui_steps_component_tests`，`-R '^rynui.steps_component$'` 各 1/1。

- 验证四状态推导/覆盖、`size_t::max` current、原始索引、当前状态 Error、真实 Text 编号及其中心、Check/Close icon、32/24 原生圆和实际连接线（包括 Error 颜色）。
- onChange 开启交互；一个 roving Tab stop；方向键/Home/End 仅移动焦点；Enter/Space/主指针显式切换；repeat/keyup 不激活；受控拒绝保持 authority 且可重试；禁用→启用后实际 pointer 命中恢复。
- 富 title/subtitle/description/icon 各挂载一次。真实子 Button pointer/key 激活与 Input focus/session/注入 composition/commit 不被 wrapper 消费。注入输入不代表系统 IME 验收。
- 横/纵、宽 `420/48/4/0`、高 `110/24/4/0` 共 32 组分配、64 个 item allocation，检查实际活动 glyph（包含 icon layers）、surface、hit clip；夹紧尺寸后居中。全隐藏时 Flex gap 重排并清理 owned focus/capture/press origin，恢复保留身份。
- 默认 current、Modal/Spin 输入边界与恢复、pointer/keyboard callback 内销毁清理全部 resources；材质更新不重测量自身/无关 sibling，空闲无待请求帧或 deadline。
- Theme 完整 colors/metrics/effects 订阅、父覆盖/inactive seed/子覆盖/独立算法；最终字体可表示范围与 Compact 派生非法度量在发布前原子拒绝。未改默认 static config、heap Prop/binding/preflight/self-reference 保护或 Windows stack。

五份 fresh golden 保留 d89eac7 的 14,032 个旧字段类型和值，仅 identity/new breadcrumb/new steps 域变化；原 28 个 IIFE 原文及顺序保持。锁定 upstream HEAD 实查为 `4a39f54842eade4e565ab336ef6097cd7e723cdd`。format/doctor/strict/diff 在阶段提交前通过。

Gallery、原生 GPU/Linux、系统 IME 和辅助技术未验收；下一阶段做共同回归与文档示例编译，独立 Astra 审查由父线程安排。
