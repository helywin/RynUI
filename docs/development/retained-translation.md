# 保留子树的位移所有权

`runtime::Node::translation` 是 logical 窗口坐标中的绝对位移。内部容器若自行组合滚动/固定层的子节点位移，在根节点设置私有标记 `owns_child_translation`。祖先的递归 writer 必须先把继承位移写到该根，再停止递归；这个标记不提供自动局部变换。

拥有者在正常同步中以“根收到的位移 + 自己的局部 offset”定位后代。嵌套拥有者按祖先到后代顺序处理；新挂载的后代也进入该过程。使用 `NodePropertyWriter`，数值未变化时不请求帧。不得先重置整个子树、再恢复局部位移，或靠持续重绘补偿。

该标记与 `clip_boundary` 分离：祖先 clip、交互边界、opacity 及独立 paint opacity 规则照常成立。标记不改变 logical scene/GPU ABI，不接入 renderer。默认 false；销毁节点和 Node slot 复用通过原有默认初始化清除标记，失败的拥有者构造也须销毁已创建节点。

当前祖先 writer 位于 Modal、Tabs、Selection 内容和 Gallery scroll。共同 CPU 合同为 `rynui.translation_ownership`，仅链接 Runtime；具体容器仍需验证非零偏移、布局/命中/clip、隐藏/退役和 idle。Table 的完整集成回归在其独立阶段提供，其他拥有者不依赖 Table API。
