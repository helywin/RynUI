# Tour 共享输入约束 Linux 来源验收

这是隔离来源 `999e57b106322afd635ff90f9cc4256cd28d8d82` 的记录，不代表 Tour 已实现；main 的重新验证见 [共享输入集成](shared-input-integration.md)。

来源基线 `20e67d510535d5accd23c1ef70989fa1ef7a3aa0`，问题最初快照 `cee72c95`，精确 Collapse 依赖后快照 `28e50875`；本阶段建立在独立 QRCode 交接 `041b3de8` 上，代码与 Theme 不含未完成 Tour。Astra 的六组旧实现反例及 4118 条 policy 模型断言是诊断，不计入本阶段 C++ 通过数。原 probe 保持不变。

- Linux GCC，正式 `linux-gcc-headless` preset、Ninja Multi-Config、Debug/Release，构建相关组件、输入与 portable targets，最大 `-j2`。定向共享/既有合同 Debug **57/57**、Release **57/57**；最后增加 retained controls 分支隐藏时 child Modal 同次退权后，Debug 受影响子集 **29/29**，Release 57 项包含该最终修改。不是全量 CTest/旧窗口矩阵。
- 新增 **14 类 / 每配置 140800 个 C++ 断言**：多 owner/legacy 幂等、4096 组集合代数、跨窗同 ID/foreign thread/代际退休、实际 pointer/focus/text/IME/candidate/activation callback 实时门禁、原子 publish/分配失败/回调抛出与 32 次收敛界限、owner scope 保留 storage 清理、wheel/Tab/ref、participant 地址复用、更新 focus 意图优先。实际 Modal 内 target 与外部 controls 可交互，其他背景拒绝；child Modal 保持 innermost，关闭/隐藏/恢复精确交还 authority，lease/grant/legacy blocks 归零。
- 用户产品决定已落实共享合同及自动回归：保留 Modal 限制，真正位于 Modal 外的 Tour target 不可交互/不可用；controls 仅通过 exact owner 的 companion 授权。`effective_input_domain()` 查询基础 Modal 域，不能把代表根当作完整复合允许域。Form available/observer/host 未改，由主线局部 registeredItem/exact-ticket helper 适配此查询。
- 热路径三种状态各 **40000 次实际 hit queries、0 allocations、20000 branch checks、0 snapshot rebuild/frame requests**，保留 geometry-before-branch；一次实际切域 7 allocations、1 frame request，后续 50 次稳定 prepare 无新增帧/deadline。原有 input scene allocation 的完整 20000 迭代严格断言仍通过。最初旧 pointer/focus lifecycle 因清理事件被过度过滤失败，已修正 ordinary input 与 cancel/leave 的区别，未放宽旧断言。
- `linux-gcc` 的 `rynui_text_input_atomicity_tests` Debug/Release 各 **1/1**，覆盖 SDL 文字事件适配；这是自动单元合同，不是系统 IME 或实际窗口证据。相关 Image/Upload/FloatButton/Carousel/ColorPicker/TreeSelect/Gallery 与实际 HEADLESS Core include/link guard 均包含在上述定向套件。
- catalog check/self-test、生成 source check、clang-format 22.1.3、`openspec doctor --json` healthy、strict **107/107**、`git diff --check` 通过。没有 Registry prune、全局 bool 补丁、Form predicate admission、renderer/SDL Core 依赖、新 translation phase 或 GPU ABI 改动。

原始日志、探针及产物只在忽略的 `out/qrcode-tour/` 和 `out/astra-tour-input-constraints/`。本阶段不关闭 45.*；Tour 实际组件/Gallery/GPU 合同另行完成。Windows/MSVC/D3D12、物理 IME/AT 与真实跨显示器未验证。
