# 保留位移统一 phase：Linux 来源平台通用结果

这是隔离来源提交 `83e6fa6ba28fef432a7652f7004ebfe62991c050` 的验证记录；当前 main 的实际组合见 [phase 集成](retained-translation-phase-integration.md)。

- 来源：`c801bbc8858259d9136a6f574cf8c4975d5380f4` 的 `owns_child_translation` 为前置依赖；本阶段只追加共同调度，不包含 Table 或 Listy 功能及完成项。
- 新增默认空 `synchronize_retained_translations` 和收集期 `enqueue_retained_translation(owner, callback)`；全部 layer writer 后、glyph 同步前按真实祖先深度应用。精确 generation、重复拒绝、stale 跳过、异常准确退役，无重置/恢复或定时轮询。
- Linux x86_64 / GCC / `linux-gcc-headless` / Ninja Multi-Config：`rynui_retained_translation_phase_tests -j2`，CTest `^rynui.retained_translation_phase$` **Debug 1/1、Release 1/1，0 失败**。
- 合同实际覆盖正/反 participant 顺序、两层 ownership、晚于 owner 的祖先 layer writer、真实 glyph/Input 位移、父级移动、原生焦点、重复/过期工作、异常清理与静止无新增 frame requests。新 phase 的测试不调用 Table/Listy API。
- HEADLESS CPU 合同；无 GPU、Windows/MSVC、系统 IME、AT 或跨实际显示器结论。原始日志、测试程序和中间产物仅在忽略的 out。
