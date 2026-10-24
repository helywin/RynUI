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

当前项或未提供 `onActivate` 只取消导航项自身的导航/Tab 资格，其中启用的原生 Button/Input 仍可点击、聚焦和编辑。项的显式 `disabled`、隐藏，以及 Modal/Spin/窗口失活边界会屏蔽整支输入；恢复保留子控件身份。成为当前项不取消已有子 Input 的输入会话。

外扩焦点轮廓使用有效的外部祖先裁剪，留有空间时四周可见；标题、图标、富内容、表面和命中仍严格受内容分配范围限制。零宽或零高不绘制焦点轮廓。

隐藏中间项不留 separator 空隙；全部隐藏或空 items 时组件移出普通布局流，恢复时保留身份。有限宽高下裁剪文字、富内容与命中，尺寸变化不会重挂载。Theme 使用 `ThemeConfig::breadcrumb` 的 `tokens`、`seed`、`algorithm`，颜色与度量分域失效；外部布局使用 `LayoutStyle`。

首版提供固定水平排列，不自动换行/响应式/RTL，不提供 router/href/params 或内置 dropdown，也不建立 Web 辅助技术桥接。验证范围为 Windows MSVC headless 的逻辑场景/输入，不代表原生 GPU 或系统 IME 验收。

## Gallery

Gallery 目录的 Breadcrumb 跳转到 `gallery.breadcrumb.trail`。样例提供首页图标、自定义分隔符、祖先改名/显隐、层级锁定与外部恢复，激活反馈显示原始 key/index。当前项内 Input 和“应用草稿”按钮独立编辑；下方无导航处理器的展示层级也保留可编辑 Input。“恢复层级”保留草稿，“编辑草稿”通过原 InputRef 恢复焦点。

平台通用 Recording 命令：`out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --navigation-only`。范围为真实目录点击、原生 Pointer/Key/Text/选区事件与三主题、双宽有限视口下的 logical scene/身份验证；执行结果见 [082 证据](../openspec/changes/082-20261025-add-navigation-gallery/evidence/01-gallery.md)。这不增加固定拓扑之外的框架能力，不代表 GPU、原生窗口、Linux、系统 IME 或辅助技术验收。
