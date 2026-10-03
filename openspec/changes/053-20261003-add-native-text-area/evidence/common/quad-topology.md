# 等量 glyph 的 IME 提交与 retained quad 范围修复

日期：2026-10-03。Windows 真窗口发现：多行 preedit `预\n编` 提交为 `中\n文` 时，glyph 总数和 draw ranges 可保持相同，而 composition underline 的 quad 数量减少。RetainedSurfaceService 已 remap fragment binding，但 WindowComponentServices 只依据其他参与者的结构标记重建 OrderedScene，仍保留末尾越界 quad command，SceneBackend 正确拒绝 attach。

ComponentSceneComposer 现在记录实际 fragment binding 变更；相同 commands/interaction/clip 为无分配 noop，发布/撤销变更后 needs_rebuild 为 true，完成 rebuild 后清除。Window 同步也检查此标记。共享库/GPU ABI 无变化，不通过 renderer 放宽范围检查。

TextArea 回归用两个相邻编辑器，在 glyph 数量相同的 preedit/commit 之间检查一次结构重建、所有 quad command 在实例范围内、随后空闲同步不重复 rebuild。

最终共同回归在 Windows/MSVC `windows-msvc-headless`：Debug 87/87（157.36 s），Release 87/87（26.44 s）。相关 `windows-msvc` 原生 CTest 含 Gallery frame 合同：Debug 16/16（41.18 s），Release 16/16（13.18 s）。日志分别为 `out/053-topology-full-headless.log` 与 `out/053-topology-native.log`。clang-format 22.1.3 全部 437 个自有源码检查 0 failures，doctor healthy，strict validate 53/53。

真实 D3D12 窗口修复后通过；详细十轮证据由 Windows 阶段独立记录。此回归并不代替 Linux 运行。
