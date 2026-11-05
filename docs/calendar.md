# Calendar

`Calendar(CalendarProps)` 是原生公历面板；使用 `CivilDate` 的 1..9999 年业务日期，不进行 epoch、UTC 或时区转换。默认值优先取 `value`、`defaultValue`、显式 `clock`，全部缺省时为 0001-01-01。`showToday` 必须配合显式 clock；时钟在挂载时提供 today 标记，Today 激活时重新取值。

固定对照 [Ant Design 6.6.5 Calendar](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/calendar/generateCalendar.tsx)、同版本 Header、style 及 basic/card/notice/calendar-select/customize-header/lunar demo。源码副本仅放 out。

| 上游合同 | C++ 原生映射 |
| --- | --- |
| value/defaultValue、mode | `Prop<CivilDate>`、`CalendarMode::Month/Year`；增加独立 defaultMode |
| fullscreen、validRange、disabledDate | 响应属性；闭区间 `CalendarRange` 与可替换 `DisabledDate` |
| locale、showWeek | 复用 `PickerLocale` 的月名、周首日、首周规则和显示年偏移；周编号包含 week year |
| cellRender/fullCellRender | `CalendarCellRenderer` 接受 `Binding<CalendarCellInfo>`，返回 typed CalendarCell；fullCellRender 替换默认标签 |
| dateCellRender/monthCellRender | 分别作为日期/月默认扩展 slot；优先级 full > common > date/month |
| headerRender | 可响应替换的 `CalendarHeaderRenderer`，通过 `CalendarHeaderContext` 提供响应 info 和 owner/thread checked CalendarRef |
| onChange/onPanelChange/onSelect | typed 回调；跨面板时 panel → change → select；相同日期只 select；source 为 Date/Month/Year/Customize |
| nativeElement | `CalendarRef::focus/select/changeMode`；不提供 DOM 指针 |
| classNames/styles | CalendarThemeConfig / CalendarTokenOverride；不提供 CSS 入口 |

受控交互发送请求，应用保留 value/mode 权威。键盘箭头按日/月份移动焦点，PageUp/Down 切换月份（Shift 或 Year 模式切换年），Home/End 到本周/年边界，Enter/Space 选择。子编辑器与嵌套 popup 自己处理输入；Calendar 不截取它们的箭头和组合输入。

日期/月作为稳定 key。相邻网格重叠日期的富 Input、Button、Popover 不重建；替换对应 renderer identity 才替换该类 cell。工厂异常回滚本次新增 cell；离开网格、禁用、隐藏及卸载清理 focus/capture/IME 与子浮层。无效边界外的网格位置保持空白。Header 内置 Select 使用 FormPrivateEditorScope，不占用外部 FormItem 的自动 control binding；用户 slot 内显式 FormItem 仍独立工作。

Calendar 通过独立颜色、尺寸、字体、阴影和焦点 token 绘制，支持 scoped seed/algorithm/inherit；LayoutStyle 仅控制外部尺寸。全屏与 mini 随可用宽度排布，RTL 镜像列与 header 原生布局；富内容按真实字体和自然尺寸测量并裁剪到 cell。

公开可编译例子见 `examples/calendar_example.hpp`。Gallery 为独立 Calendar 页面，提供真实年月选择、模式切换、Today、选择/面板回调、周编号和保留日程 Input。

不包含 Web DOM/CSS/React、浏览器 portal 或 OS 辅助技术声明。HEADLESS 的 Core/逻辑场景、Linux 私有 Wayland GPU 和实际桌面/系统 IME/Windows 是不同证据边界。
