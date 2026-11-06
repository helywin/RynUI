# Text margin 后的约束恢复

修复独立 Text 从正常 margin 切换到占满可用高度、再恢复时继续复用零高度量测的问题。LayoutEngine 保存实际量测约束；Text 仅在约束仍匹配时使用 placement 快速路径。普通 margin 更新仍保留节点、字形及原有最小失效合同。

实际 Windows/MSVC，`windows-msvc-headless`，Ninja Multi-Config、最多 2 compiler。Debug 与 Release 的 `rynui.portable.text_component`、`rynui.portable.layout_engine`、`rynui.portable.layout_style` 各 **3/3 通过**。新增回归检查零高度往返后高度恢复、Component 身份不变且无额外 shaping。

clang-format 22.1.3、OpenSpec doctor、strict **108/108**、diff 检查通过。本阶段属于 CPU 布局合同，不据此声明 GPU、Linux 或系统输入验收。
