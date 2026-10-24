# Steps

固定 `std::vector<StepsItem>`，key 非空且唯一。`current` 为受控 `Prop<std::size_t>`，`defaultCurrent` 为默认索引，两者互斥；默认 0。`current >= count` 表示全部完成，不回写 current。单项 `status` 优先于按原始索引派生的状态。只有 `onChange` 存在才启用导航，受控拒绝可重试。

标题、副标题、说明、禁用、可见和状态响应式更新；typed 富 slots 只挂载一次。拓扑改变需要重新挂载。

在窗口 `Content` 中调用。以下受控示例由应用明确接受请求；保留 `current` 值即可拒绝，后续显式激活可以重试。函数体作为 API 示例编译验证：

```cpp
#include <ryn/rynui.hpp>

void steps_example(ryn::Signal<std::size_t>& current, ryn::Signal<bool>& review_visible) {
    using namespace ryn;
    Steps(StepsProps{}.current(current).size(StepsSize::Small)
              .onChange([&current](std::size_t next) { current.set(next); }),
          {StepsItem{u8"details", u8"Details"}.description(u8"Enter details"),
           StepsItem{u8"review", u8"Review"}.subtitle(u8"Optional").visible(review_visible)
               .icon(StepsIcon{[] { Icon(IconProps{}.name(IconName::SearchOutlined)); }}),
           StepsItem{u8"complete", u8"Complete"}.richDescription(StepsDescription{[] {
               Text(u8"Ready to submit");
           }})});
}
```

`StepsProps{}.defaultCurrent(1).onChange(callback)` 使用内部 current authority；不设置 `onChange` 时为展示组件。`status(Prop<StepsStatus>)` 控制当前步骤状态，`StepsItem::status(Prop<std::optional<StepsStatus>>)` 用 `nullopt` 恢复自动推导。状态为 `Wait`、`Process`、`Finish`、`Error`，默认当前状态 `Process`。编号始终为原始索引加一，隐藏项不使后续步骤改号或改变 callback index。

`vertical(true)` 纵向排列，默认横向；`StepsSize::Default` 与 `Small` 两尺寸。横向均分可用宽度，纵向按内容排列；编号、check/error、状态圆和连接线是原生 logical scene。`richTitle`、`richSubtitle`、`richDescription`、`icon` 都是独立 typed slots。原生子 Button/Input 保留自己的激活、编辑及输入会话。

只有一个可见启用步骤作为 roving Tab stop。方向键（横向 Left/Right、纵向 Up/Down）及 Home/End 仅移动焦点；Enter/Space 和主指针显式请求切换。repeat/keyup 不额外激活，激活当前索引不重复回调。Modal、Spin、隐藏项和失活边界清理不可用交互；全隐藏或空 items 时不占普通布局位置，恢复保留身份。

Theme 使用 `ThemeConfig::steps` 的独立 `tokens`、`seed`、`algorithm`；颜色更新不重测量，最终派生度量先校验再发布。`LayoutStyle` 只控制外部布局。有限宽高下裁剪实际文字、富内容、表面与命中。

首版不提供动态拓扑替换、dot/inline/navigation/panel/percent/maxCount、自动 responsive/RTL、动画或 Web 辅助技术桥接。验证范围为 Windows MSVC headless，不代表 Gallery、原生 GPU、Linux 或系统 IME 验收。
