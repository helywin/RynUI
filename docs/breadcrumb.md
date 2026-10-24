# Breadcrumb

固定 `std::vector<BreadcrumbItem>`，key 非空且唯一。项内 `title`、`disabled`、`visible` 使用 `Prop<T>`，富标题、图标和 separator 使用独立 typed slots。最后一个可见项是当前项，祖先激活回调返回原始 key/index。隐藏项不改变公开索引。拓扑改变需要重新挂载。

在窗口的 `Content` 中调用；导航由应用自己的回调处理。以下函数体作为 API 示例编译验证：

```cpp
#include <ryn/rynui.hpp>

void breadcrumb_example(ryn::Signal<ryn::String>& title,
                        ryn::Signal<bool>& details_visible,
                        std::function<void(ryn::BreadcrumbActivation)> navigate) {
    using namespace ryn;
    Breadcrumb(BreadcrumbProps{}.onActivate(std::move(navigate))
                   .separator(BreadcrumbSeparator{[] { Text(u8">"); }}),
               {BreadcrumbItem{u8"home", u8"Home"}
                    .icon(BreadcrumbIcon{[] { Icon(IconProps{}.name(IconName::HomeOutlined)); }}),
                BreadcrumbItem{u8"list", u8"Records"}.title(title),
                BreadcrumbItem{u8"detail", u8"Details"}.visible(details_visible)});
}
```

`richTitle(BreadcrumbTitle)` 替代默认标题文字；`icon(BreadcrumbIcon)` 放在标题之前。slots 保留原身份，普通属性更新不再次执行 slot。未提供 `onActivate` 时只展示层级。禁用项不发导航请求；当前项为非导航文字。祖先项通过 Tab 聚焦、Enter/Space 或主指针激活；repeat/keyup 不额外激活。富 slots 中的 Button/Input 使用自己的输入行为。

隐藏中间项不留 separator 空隙；全部隐藏或空 items 时组件移出普通布局流，恢复时保留身份。有限宽高下裁剪文字、富内容与命中，尺寸变化不会重挂载。Theme 使用 `ThemeConfig::breadcrumb` 的 `tokens`、`seed`、`algorithm`，颜色与度量分域失效；外部布局使用 `LayoutStyle`。

首版提供固定水平排列，不自动换行/响应式/RTL，不提供 router/href/params 或内置 dropdown，也不建立 Web 辅助技术桥接。验证范围为 Windows MSVC headless 的逻辑场景/输入，不代表原生 GPU 或系统 IME 验收。
