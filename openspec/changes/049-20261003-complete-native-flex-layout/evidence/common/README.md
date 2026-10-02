# Flex 平台通用合同

2026-10-03，Windows MSVC、Ninja Multi-Config、`windows-msvc-headless` Debug/Release。第一阶段 LayoutEngine/LayoutStyle/Flex/Space/public API 受影响 CTest 各 10/10（0.59s/0.46s）。这些既有 CPU fixtures 新增到 HEADLESS preset，不依赖 SDL；后续完整 HEADLESS suite 为 64 项。

新合同覆盖 H/V reverse-wrap 与 RTL 异或方向、物理 Left/Right、placement-only 与测量次数/child 顺序/挂载次数保持、非法 typed wrap/direction 回滚、bool reactive wrap 兼容及销毁解绑。布局未修改 interaction/focus registry。

408 自有 C++/HLSL 通过 clang-format 22.1.3，OpenSpec doctor healthy、strict 49/49、Git diff 检查通过。真实字体、窗口/GPU、输入和 DPI 结果分别记录。
