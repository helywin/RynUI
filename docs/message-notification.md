# Message 与 Notification

在窗口 content 中挂载 holder，用 Ref 从按钮等事件中打开提示。Ref 可按值捕获；holder 卸载后，命令返回 false。每个 Ref 同时只能绑定一个 holder；可以在旧 holder 卸载后重绑。所有操作在所属 UI 线程进行。

```cpp
#include <ryn/rynui.hpp>

void FeedbackControls() {
    ryn::MessageRef messages;
    ryn::NotificationRef notifications;
    ryn::MessageHolder(ryn::MessageHolderProps{}.ref(messages));
    ryn::NotificationHolder(ryn::NotificationHolderProps{}.ref(notifications));
    ryn::Button(ryn::ButtonProps{}.onClick([messages, notifications] {
        static_cast<void>(messages.open(
            u8"save", ryn::MessageProps{}.content(u8"Saved").status(ryn::NoticeStatus::Success)));
        static_cast<void>(notifications.open(
            u8"job", ryn::NotificationProps{}.title(u8"Export").description(u8"Ready").duration(0),
            {.actions = ryn::NotificationActions{[notifications] {
                 ryn::Button(ryn::ButtonProps{}.onClick([notifications] {
                     static_cast<void>(notifications.close(u8"job"));
                 }), [] { ryn::Text(u8"Dismiss"); });
             }}}));
    }), [] { ryn::Text(u8"Save"); });
}
```

在已有 Window content slot 中调用 `FeedbackControls()`。holder 归该 Window 与其逻辑祖先所有，多个窗口的 Ref 不共享状态。`open` 需要非空 key，返回值表示该次命令结束时对应 entry 仍存活；同步回调卸载/替换时可能返回 false。`close(key)` 返回是否关闭了该 entry；`clear()` 返回 holder 是否仍可接收该命令。

Message 支持 `content(Prop<String>)`、`status(Prop<NoticeStatus>)`、`duration(Prop<double>)`、`onClose`，富内容为 `MessageSlots::content` / `MessageContent`。Notification 支持 `title`、`description`、`status`、`placement`、`duration`、`closable`、`onClose`；富内容为 `NotificationTitle`、`NotificationDescription`、`NotificationActions`。placement 包含 TopLeft、TopRight、BottomLeft、BottomRight。所有状态 Success、Info、Warning、Error、Loading 均有实际图标和 Theme 颜色；Loading 首版是静态图标。

同 key `open` 只更新明确提供的字段，不改变 entry 的插入顺序。未提供的属性连接和 slots 原样保留。传入新的 typed slot 会只替换该区域；传入空 slot 函数可清空该富内容区域。字符串更新不会移除已有富内容；更新富内容里的 Text 请把 Signal 绑定到该 Text。其他 entry、未替换区域中的 Input 编辑器、草稿与 ref 不重挂。原有 slot 的 Theme scope 继承 holder；颜色、尺寸和位置变化沿现有 reactive 更新链处理。

holder 的 `maxCount(Prop<std::size_t>)` 默认为 8，允许 1..1024；超限关闭最早插入的条目，包括 duration=0 条目。reactive 减小 maxCount 使用相同策略。close、clear、到期和淘汰会在销毁后调用对应 onClose 一次；holder/window 卸载只清理资源，不调用 onClose。close/clear/到期批次使用完整 entry generation，回调新建的同 key 条目不会被旧批次删除。回调可同步卸载、更新其他条目或重绑已卸载的 Ref。挂载中的 slot 遵守 Runtime 的既有约束，不能重入挂载；可同步卸载自身。slot 抛异常时保留未替换的旧 region。

duration 单位为秒，Message 默认 3、Notification 默认 4.5；0 持久，正数到期关闭。允许有限非负值，最大十年。每次同 key open 都按更新后的当前 duration 重新计时；reactive duration 改变也重新计时。使用窗口的非阻塞单调 tick/deadline，过期时间不因时钟倒退而提前。hover 和窗口失活均继续计时；重新处理时间时关闭已到期条目。无 entry 或只有 duration=0 的静态内容时，notice 不安排持续帧。首版没有动画、进度条、pauseOnHover、全局 service 或 promise API。

Message 顶部居中；Notification 四角向内堆叠，标题、描述、真实关闭 Button 和 action 区域分开保留。宽高约束到当前窗口，超长内容裁剪；内容内的 TextArea/Select 等保留已有输入与滚轮功能，首版没有额外通知滚动容器。open 不请求焦点；关闭 focused 内容安全结束输入会话并清理焦点。Modal、Spin 与隐藏祖先的输入边界仍生效。notice 仅在子控件处理后消费自己实际顶层命中的滚轮回退，不处理 Escape，不吞被覆盖 sibling 或子 popup 的事件。

`ThemeConfig::message` / `notification` 分别支持 `tokens`、`seed`、`algorithm`。颜色为 background/text/description/success/info/warning/error/loading；指标为 width、padding_block、padding_inline、gap、stack_gap、offset、border_radius、font_size、line_height、title_font_size、title_line_height、icon_size、close_size、font_family、title_font_weight，另有 shadow 与 z_index。视觉只由 Theme 控制；holder 不占正文布局。默认 recipe 对照本地锁定 Ant Design 6.6.5（4a39f54842eade4e565ab336ef6097cd7e723cdd），不引入其运行时依赖。

自动验收是 Windows MSVC headless 的逻辑布局、场景、事件、clock 和生命周期测试；不构成 GPU/native 窗口/Linux/system IME/AT 验收。
