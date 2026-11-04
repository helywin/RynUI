# Breadcrumb

支持原有 `Breadcrumb(props, std::vector<BreadcrumbItem>)` 及 `Breadcrumb(BreadcrumbProps{}.items(Prop<std::vector<BreadcrumbItem>>))`。key 非空且唯一；插入、移除、重排与响应属性变化保留未改变条目和富内容。每项 `title`、`disabled`、`visible` 使用 `Prop<T>`，富标题、图标和分隔符使用独立 typed slots。最后一个可见项是当前项，激活返回当前拓扑的 key/index；隐藏项保留其索引。

设计对照 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 Breadcrumb/BreadcrumbItem 类型、API、overlay 与 separator demos。原生事件取代路由跳转。

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

`richTitle(BreadcrumbTitle)` 替代默认标题文字；`icon(BreadcrumbIcon)` 放在标题之前。slots 保留原身份，普通属性更新不再次执行 slot。未提供全局或每项 `onActivate` 时只展示层级；每项处理器优先于全局处理器。禁用项不发导航请求；当前项为非导航文字。祖先项通过 Tab 聚焦、Enter/Space 或主指针激活；repeat/keyup 不额外激活。富 slots 中的 Button/Input 使用自己的输入行为。

当前项或未提供 `onActivate` 只取消导航项自身的导航/Tab 资格，其中启用的原生 Button/Input 仍可点击、聚焦和编辑。项的显式 `disabled`、隐藏，以及 Modal/Spin/窗口失活边界会屏蔽整支输入；恢复保留子控件身份。成为当前项不取消已有子 Input 的输入会话。

外扩焦点轮廓使用有效的外部祖先裁剪，留有空间时四周可见；标题、图标、富内容、表面和命中仍严格受内容分配范围限制。零宽或零高不绘制焦点轮廓。

隐藏中间项不留 separator 空隙；全部隐藏或空 items 时组件移出普通布局流，恢复时保留身份。有限宽高下裁剪文字、富内容与命中，尺寸变化不会重挂载。Theme 使用 `ThemeConfig::breadcrumb` 的 `tokens`、`seed`、`algorithm`，颜色与度量分域失效；外部布局使用 `LayoutStyle`。

默认 `wrap(true)` 按可用宽度换行，条目及其分隔符作为一个布局单元；`wrap(false)` 保留旧版有限宽度压缩布局。`direction(TextDirection::RightToLeft)` 同时镜像条目、图标和分隔符，状态更新不重建编辑器。`Auto` 当前使用 LTR 回退。

`BreadcrumbItem::separator` 覆盖该项后的分隔符，未指定则使用全局 separator。`menu(MenuProps)` 直接组合原生 Dropdown/Menu，保留菜单的点击、选择、禁用与键盘行为；当前项也可拥有菜单。`dropdownIcon(BreadcrumbIcon)` 替换默认 DownOutlined，`dropdownProps(DropdownProps)` 转发受控开合、触发与定位等原生配置；同时设置 menu 时以该 menu 为准。整支输入屏蔽仍由 Breadcrumb 和宿主边界统一负责。移除或替换条目会销毁其 popup、焦点、捕获和输入会话；保留键的重排继续使用原来的 slot。

不提供 router/href/params、DOM/CSS/React 或 Web 辅助技术桥接。平台通用自动合同与原生 GPU、Windows/Linux、系统 IME、辅助技术证据分别记录。

## Gallery

Gallery 目录的 Breadcrumb 跳转到 `gallery.breadcrumb.trail`。样例提供首页图标、自定义分隔符、祖先改名/显隐、层级锁定与外部恢复，激活反馈显示原始 key/index。当前项内 Input 和“应用草稿”按钮独立编辑；下方无导航处理器的展示层级也保留可编辑 Input。“恢复层级”保留草稿，“编辑草稿”通过原 InputRef 恢复焦点。

平台通用 Recording 命令：`out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --navigation-only`。范围为真实目录点击、原生 Pointer/Key/Text/选区事件与三主题、双宽有限视口下的 logical scene/身份验证；执行结果见 [082 证据](../openspec/changes/082-20261025-add-navigation-gallery/evidence/01-gallery.md)。该历史结果不代表新增动态合同的 GPU、原生窗口、Linux、系统 IME 或辅助技术验收。

新增独立样例 `gallery.sample.breadcrumb.dynamic` 演示内置菜单、键控重排、删除恢复、保留编辑草稿、换行和 RTL；既有 `gallery.breadcrumb.*` ID 保持不变。本轮结果见 [107 Breadcrumb 证据](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/breadcrumb-linux.md)。
