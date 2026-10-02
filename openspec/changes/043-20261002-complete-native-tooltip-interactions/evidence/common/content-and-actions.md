# 富标题与动作合同

2026-10-02，Windows/MSVC 平台通用验证，Ninja Multi-Config 的 `windows-msvc-headless-debug` / `windows-msvc-headless-release`。

- focused build 成功；Tooltip / PointerRoute / Slider CTest 两配置均 3/3，Debug 1.59 秒、Release 1.18 秒。
- TooltipTitle 的 Flex/Text/Icon 挂载一次、语义继承和 reactive title 尺寸调整；titleAvailable、String/slot 冲突、trigger/组合冲突与 slot 异常 rollback。
- Click 保留 Button 激活；未回写 controlled 连续请求 true/false、hover/focus 关闭抑制、ContextMenu logical 指针锚点、移动时锁存、Escape/配置切换/disabled；空白关闭、popup 内点击、drag 到外部不切换、嵌套提示与子 Button/提示回调销毁。
- PointerRouter post-route observer 覆盖 stop propagation 后观察、空白 hit、primary origin、回调替换、自身重入拒绝/异常 pointer abort、失效 hit 与 owner-thread setter；窗口参与者 scratch 在 attach 时预留，稳定派发不为 participant 快照分配。
- 为 popup 加 passive hit，阻止标题背后控件接收同一次 click；不夺取 focus。Slider 资源清理合同相应计入每个提示的 popup hit。
- clang-format 22.1.3 检查 395 个文件、0 failures；doctor healthy、full strict 43/43、git diff --check 通过。公开说明见 docs/tooltip.md。

仅平台通用合同已通过；043 glyph 箭头、Gallery 整合和 Windows/Linux native 平台验收仍属于后续任务。
