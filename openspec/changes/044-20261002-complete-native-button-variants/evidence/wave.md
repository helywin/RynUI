# Wave 阶段证据

2026-10-02，Windows x64，MSVC / Ninja Multi-Config，任务 4.1 平台通用实现完成。

- `windows-msvc-headless-debug`：Button/Input/Selection/Slider/Tooltip focused CTest 5/5，5.77 s。
- `windows-msvc-headless-release`：相同 focused CTest 5/5，3.99 s。
- clang-format 22.1.3 check：397 个源文件，0 failures。
- doctor healthy；full strict 44/44；diff check 通过。

默认 activation 使用一个额外 scalar AnimationRuntime target 和共同 RoundedEffect outline；Theme wave_spread/width/opacity 与 slow/ease_out motion token 控制扩散/淡出，hash/诊断与 Material/Geometry identity 纳入新值。普通按钮保持旧颜色路径；默认中性色 wave 使用主题 hover 主色，避免白色背景反馈不可见。新 target 不在 idle 时播放。

测试覆盖 keyboard/pointer activation（含既有 pointer 合同）、重复 activation 重启单一 effect、期间不测量/不重跑/不改变焦点和命中、主题几何/淡出值、结束 effect/deadline 清理；wave=false、disabled/loading、Text/Link、motion=false/reduced、窗口失活、owner destruction 与 destructive activation callback 均能清理。新生命周期测试记录实际 6 通道资源数量，未关闭产品默认 wave 来通过旧回归。

内部 range 由组件保留复用，终止时 effect 列表为空，销毁时 range 和 target 全部释放。未修改 GPU ABI 或加入私有平台渲染。真实 Windows/Linux native 验收留待后续任务；开发日志为 `out/044-wave-tests.log`。
