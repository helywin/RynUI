# 整段轨道阶段

2026-10-02，Windows/MSVC，平台通用合同使用 windows-msvc-headless Debug/Release；不作为 native GPU 验收。

双端/多端整段轨道拖动已实现：起始快照、首点对齐、共同边界夹取和逐点归一化。规则 step、irregular marks、horizontal/vertical/reverse、controlled no echo/echo、配置变化、window loss 和 reentrant destroy 合同通过。互斥配置在资源取得/状态变化前拒绝。

Debug/Release focused CTest 的 slider_component、tooltip_component、selection_component、interaction_registry 通过。clang-format 22 检查、doctor/full strict 与 git diff --check 通过。editable/disabled/ref 仍待下一阶段。
