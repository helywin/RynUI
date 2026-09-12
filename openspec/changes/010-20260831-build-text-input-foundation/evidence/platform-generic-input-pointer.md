# 平台通用：Input 指针编辑

2026-09-12；任务 7.3；Windows/MSVC `windows-msvc-debug`。

## 实现

- 复用 PointerRouter 的 target stage、FocusManager 和 capture。每个 Input 同时只有一个 selection pointer；其他 pointer 不得抢走 anchor，hover 仍按多 pointer 计数。
- 点击使用 revision-bound TextCaretMap nearest grapheme stop；当前 scroll offset 参与坐标换算。Props 更新与 pointer 同批时刷新失效的 caret map，不使用旧文本 revision。
- 拖动保留 anchor，离开窗口仍通过 capture 更新；release 使用最后坐标并释放，cancel 不再修改 selection。focus loss、window loss、disable、destroy 清理 capture。
- double-click 使用既有 logical word selector；相同位置 move/up 不把 word selection 折叠。prefix/suffix/padding 只允许 focus，不改变文本 selection；ancestor clip 外不接受起始点击。
- composition display byte 先映射回 committed byte，再取消 preedit 并定位；selection 不产生 onChange 或 history transaction。
- 平台中立 PointerInputEvent 增加 click_count；SDL adapter 复制已锁定 SDL 3.4.14 `SDL_MouseButtonEvent::clicks`。本地 `SDL_events.h` 与官方 SDL Wiki/Context7 一致：1 为单击、2 为双击；不自行设置 OS 双击阈值。

## 验证

Input component、Input pointer、SDL adapter、Pointer route/state/lifecycle/allocation、Focus order/state/lifecycle/allocation 定向 11/11 通过（3.66 秒）。随后增加四档 1.0/1.25/1.5/2.0 simulated DPI、ancestor clip 和同批 controlled text 更新测试，Input pointer 再次通过（1.07 秒）。

测试包含 Latin/ligature、CJK、combining、emoji ZWJ、empty boundary 语义、word selection、preedit cancel、read-only/disabled、多 pointer、outside release/cancel、window loss 和 destroy。20,000 次真实 PointerRouter 拖动并同步场景，预热后 0 C++ heap allocation，shaping/measure/composer rebuild 不增长。

这不是 native pointer/IME、真实 GPU、系统 DPI 或 Linux 机器验收；这些清单保持独立。随后该指针阶段的完整 CTest 198/198 通过（277.44 秒），其中 256 Input 基准通过（155.51 秒）。此回归不包含后续键盘实现。
