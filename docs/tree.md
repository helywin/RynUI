# Tree

固定 key 与层级的原生树，最多 1024 个节点、64 层；超限或 key 为空/重复会抛出 invalid_argument。拓扑在挂载后固定，标题、visible、disabled 和 Tree 状态使用 Prop。selectedKeys 仅单选，规范化时取第一个已知 key；展开 key 不自动展开祖先。

expandedKeys、selectedKeys、checkedKeys 各自与对应 default 属性互斥。受控交互只发候选，实际状态等待调用方更新。checkbox 非 strict 级联传播到可用子节点，disabled 分支隔离，不参与父节点全选/半选汇总；显隐不改变勾选拓扑。

```cpp
#include <ryn/tree.hpp>

void project_tree() {
    ryn::Tree(ryn::TreeProps{}.checkable().defaultExpandedKeys({u8"project"})
                  .layout(ryn::LayoutStyle{}.width(ryn::dp(320)).height(ryn::dp(200))),
              {ryn::TreeItem{u8"project", u8"Project",
                             {ryn::TreeItem{u8"src", u8"Sources"}, ryn::TreeItem{u8"docs", u8"Docs"}}}});
}
```

richTitle(TreeTitle{...}) 和 icon(TreeIcon{...}) 挂载一次并保留原生内容身份。禁用节点仅阻止 Tree 行自身操作，富 child 遵循自己的 disabled 状态；全树 disabled 会阻止整树输入。隐藏/折叠会取消后代输入及 popup，恢复不会重开旧 popup。

Tree 行单 Tab 入口；Up/Down、Home/End 移动，Right 展开或进入子项，Left 折叠或到父项，Enter 单选，Space 在 checkable 时勾选，否则单选。富 child 保留自己的 Tab 和键盘行为。有限高度下滚轮和键盘显露焦点；不虚拟化。视觉仅由 ThemeConfig.tree.tokens 控制，LayoutStyle 仅控制外部布局。

首版不提供多选、strict checking、动态拓扑、异步 loadData、拖放、虚拟化、搜索、RTL、动画、连接线、DirectoryTree 或 Web API。验证仅覆盖 HEADLESS logical scene 与原生输入运行时，不代表系统 IME、辅助技术、GPU 或 Linux 验收。
