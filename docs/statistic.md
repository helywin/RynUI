# Statistic

原生统计值展示。合同锁定 Ant Design 6.6.5（`4a39f54842eade4e565ab336ef6097cd7e723cdd`）的 Statistic、Number 和 Component Token；实现使用 RynUI retained 布局、文本场景和 Skeleton。

```cpp
inline void StatisticExample() {
    ryn::Statistic(ryn::StatisticProps{}.title(u8"用户数").value(std::uint64_t{18446744073709551615ULL}).suffix(u8"人"));

    ryn::Signal<ryn::StatisticValue> balance{ryn::StatisticValue{u8"-1234.123456789"}};
    ryn::Statistic(ryn::StatisticProps{}
                       .title(u8"账户余额")
                       .value(balance)
                       .precision(8)
                       .decimalSeparator(u8".")
                       .groupSeparator(u8",")
                       .richPrefix(ryn::StatisticPrefix{[] { ryn::Text(u8"CNY"); }}));

    ryn::Statistic(ryn::StatisticProps{}.value(17).formatter([](const ryn::StatisticValue&) -> ryn::String {
        return u8"已完成";
    }));
}
```

包含 `<ryn/rynui.hpp>`（或 `<ryn/statistic.hpp>`）；在窗口 Content 内调用。示例与 `tests/support/statistic_example.hpp` 保持一致，并在真实 HEADLESS 文本场景中执行。

| Props | 合同 |
| --- | --- |
| `value` | `StatisticValue`，保存 int64、uint64、double 或 String，默认 0；Decimal 使用其精确文本进入既有 String alternative，formatter/storage 合同保持；接受对应静态值、Signal、Binding 及 `Prop<StatisticValue>` |
| `title/prefix/suffix` | `Prop<String>`，默认空；UTF-8 literal 有便捷重载 |
| `precision` | `Prop<std::optional<int>>` 或 int；默认保留原小数，负数按 0，超过 100 拒绝 |
| `decimalSeparator/groupSeparator` | `Prop<String>`，默认 `.` / `,`，允许空和多字符 |
| `formatter` | `Prop<StatisticFormatter>` 或回调；接收原始 typed value，返回 String，优先于内置格式化 |
| `richTitle/richValue/richPrefix/richSuffix` | 分别接受 `StatisticTitle/StatisticValueContent/StatisticPrefix/StatisticSuffix`，优先于对应文本；slot 仅初次挂载执行 |
| `loading/visible` | `Prop<bool>`，默认 false / true；loading 使用 Skeleton 并暂停值区域输入，标题保留；visible=false 移除 flow 与 gap |
| `layout` | 仅控制外部布局 |

整数和 Decimal 不经过 double；浮点通过 `to_chars` 最短可往返格式输出，保留负零，不限制为六位小数。内置十进制格式化使用上游的 `-?digits(.digits)?` 识别规则：小数按 precision 截断或补零，`.5` 补零整数；非十进制字符串（例如 `1e3`、`12.`、`-`）原样展示。C++ 浮点转换若产生指数形式，也原样显示。需要固定十进制位或超出 double 精度的数据时使用 String 或 Decimal。数值 NaN/Infinity 在 formatter 前拒绝；字符串内容不按数值强制转换。失败更新保留上一有效显示，后续合法更新可恢复。

富内容通过自己的 Signal/Binding 响应数据，不会因普通 value/formatter 更新重建。隐藏、loading 和零尺寸恢复保留草稿、refs 与子控件身份。有限宽按 prefix、value、suffix 顺序分配剩余宽度，各区域裁剪内容，短高裁剪字形，不缩小字体。输入与文档裁剪沿用窗口、Modal、Spin 和 Popup 的既有边界。

rich slot 挂载期间更新的 value、precision、分隔符与 formatter 会反映在首帧；没有相关更新时，首次展示只调用一次 formatter。

首次挂载失败会清理本次组件并重抛原异常，调用处可捕获后展示 fallback；回调在外部创建的替代组件保留。

`ThemeConfig::statistic` 提供 `tokens`、`seed`、`algorithm`。colors 为 `title/content`；metrics 为 `title_font_size/content_font_size/title_line_height/content_line_height/title_gap/affix_gap/loading_padding/font_family`。默认标题 14、内容 24、间距 4、loading 顶部 padding 16 logical dp；尺寸随 Theme 派生，字体最终值至少 0.5 dp，round 后的逻辑像素字号不得超过 `INT_MAX / 64`；非法最终字号在 Theme 发布前拒绝。支持继承、Dark/Compact、独立 seed、identity、诊断和颜色/尺寸独立 dirty 订阅。

`locale(Prop<StatisticLocale>)` 提供 decimal/group separators、primary/secondary group 位数和十个 UTF-8 digits；primary/secondary 均须大于零，digit 不得为空。显式 decimalSeparator/groupSeparator 分别覆盖 locale 对应字段，formatter 和非数字字符串保持原合同。locale 更新只更新保留文本。比如 primary=3、secondary=2 可得到 `12,34,567`。

未显式指定 locale 时，数字和 Timer 的 digits 继承最近 ConfigProvider 的 formatLocale，与翻译语言独立；显式完整 StatisticLocale（包括默认英文值）继续优先。数字显示算法与公开 i18n formatNumber 共用，不增加 ICU/货币功能；精确 Decimal 不参与 double 数值动画。新增国际化接入的回归源码等待父集成验证，不扩大历史验收范围。

`animated(Prop<bool>)` 默认 false。开启后，安全数值更新从当前显示插值到新值，使用 Theme slow duration 和 motion policy；结束发布原始 typed 值，不把大整数或高精度 String 经由 double 转换。自定义 formatter 与 richValue 不驱动内置数值动画。隐藏、loading、离页、零可见区域、窗口失活或 reduced motion 收敛到目标并停止 deadline。

`timer(Prop<std::optional<StatisticTimer>>)` 使用 `StatisticTimer{StatisticTimerType::Countdown/Countup, Duration, format}`。Duration 表示本次配置更新时的剩余/已过时长，随后由窗口单调时间推进，不使用浏览器 Date 或定时器。默认 format 为 `HH:mm:ss`；支持 `Y/M/D/H/m/s/S` 重复单位宽度及 `[literal]` 转义，年/月按365/30天。移除 timer 恢复原 value。

Timer 按最小可见单位安排下一次 deadline，毫秒显示最多约60Hz。`onChange(Duration)` 交付实际采样时长；Countdown 到零交付一次 `onFinish()`，Countup 不交付 finish。回调后检查完整 owner 与配置代际；销毁、重绑或改 timer 不再调用旧 finish。hidden/loading/inactive/reduced motion/off-page 停止自动采样和 callback，恢复后按单调时钟追赶；富 value slot 使用自己的响应数据，不启动内置 Timer。

107 的 Timer/locale/动画和新增回归已完成本轮 MSVC Debug/Release 与 D3D12 检查，结果见[Windows 阶段验收](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/feedback-i18n-windows.md)。DOM classNames/styles/valueStyle、OS 可访问性桥接及其他平台继续独立跟踪。

Gallery 目录选择 **Statistic**，直接进入统计指标。纯数值 Statistic 在高精度 String、double、uint64 和 int64 之间切换，支持 2/8/原精度、分隔符及 formatter；业务反馈保留原始 typed 值。第二个 Statistic 使用真实富 value Input，可保存指标备注，loading 暂停编辑并取消 preedit，外部“恢复指标编辑”保留草稿与 ref。定向入口为 `rynui_token_gallery_frame_tests --members-metrics-only`，仅覆盖 Recording 逻辑场景和合成事件，完整 Gallery 与平台验收另行完成。

独立目录页增加5秒倒计时、正计时、停止、数值动画与 locale 控件。自动动作 identities 为 `gallery.statistic.countdown/countup/timer-stop/animate/locale`，展示入口为 `gallery.statistic.timer/animated-value`；默认未启动 Timer 时不产生周期 deadline。
