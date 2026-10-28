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
| `value` | `StatisticValue`，保存 int64、uint64、double 或 String，默认 0；接受对应静态值、Signal、Binding 及 `Prop<StatisticValue>` |
| `title/prefix/suffix` | `Prop<String>`，默认空；UTF-8 literal 有便捷重载 |
| `precision` | `Prop<std::optional<int>>` 或 int；默认保留原小数，负数按 0，超过 100 拒绝 |
| `decimalSeparator/groupSeparator` | `Prop<String>`，默认 `.` / `,`，允许空和多字符 |
| `formatter` | `Prop<StatisticFormatter>` 或回调；接收原始 typed value，返回 String，优先于内置格式化 |
| `richTitle/richValue/richPrefix/richSuffix` | 分别接受 `StatisticTitle/StatisticValueContent/StatisticPrefix/StatisticSuffix`，优先于对应文本；slot 仅初次挂载执行 |
| `loading/visible` | `Prop<bool>`，默认 false / true；loading 使用 Skeleton 并暂停值区域输入，标题保留；visible=false 移除 flow 与 gap |
| `layout` | 仅控制外部布局 |

整数不经过 double；浮点通过 `to_chars` 最短可往返格式输出，不限制为六位小数。内置十进制格式化使用上游的 `-?digits(.digits)?` 识别规则：小数按 precision 截断或补零，`.5` 补零整数；非十进制字符串（例如 `1e3`、`12.`、`-`）原样展示。C++ 浮点转换若产生指数形式，也原样显示。需要固定十进制位或超出 double 精度的数据时使用 String。数值 NaN/Infinity 在 formatter 前拒绝；字符串内容不按数值强制转换。失败更新保留上一有效显示，后续合法更新可恢复。

富内容通过自己的 Signal/Binding 响应数据，不会因普通 value/formatter 更新重建。隐藏、loading 和零尺寸恢复保留草稿、refs 与子控件身份。有限宽按 prefix、value、suffix 顺序分配剩余宽度，各区域裁剪内容，短高裁剪字形，不缩小字体。输入与文档裁剪沿用窗口、Modal、Spin 和 Popup 的既有边界。

rich slot 挂载期间更新的 value、precision、分隔符与 formatter 会反映在首帧；没有相关更新时，首次展示只调用一次 formatter。

首次挂载失败会清理本次组件并重抛原异常，调用处可捕获后展示 fallback；回调在外部创建的替代组件保留。

`ThemeConfig::statistic` 提供 `tokens`、`seed`、`algorithm`。colors 为 `title/content`；metrics 为 `title_font_size/content_font_size/title_line_height/content_line_height/title_gap/affix_gap/loading_padding/font_family`。默认标题 14、内容 24、间距 4、loading 顶部 padding 16 logical dp；尺寸随 Theme 派生，字体最终值至少 0.5 dp，round 后的逻辑像素字号不得超过 `INT_MAX / 64`；非法最终字号在 Theme 发布前拒绝。支持继承、Dark/Compact、独立 seed、identity、诊断和颜色/尺寸独立 dirty 订阅。

首版不包含 Timer、Countdown、Countup、数值动画、locale 国际化、DOM classNames/styles/valueStyle。HEADLESS 测试验证逻辑场景与输入路由，原生窗口、GPU、Linux、系统 IME/AT 需另行验收。

Gallery 目录选择 **Statistic**，直接进入统计指标。纯数值 Statistic 在高精度 String、double、uint64 和 int64 之间切换，支持 2/8/原精度、分隔符及 formatter；业务反馈保留原始 typed 值。第二个 Statistic 使用真实富 value Input，可保存指标备注，loading 暂停编辑并取消 preedit，外部“恢复指标编辑”保留草稿与 ref。定向入口为 `rynui_token_gallery_frame_tests --members-metrics-only`，仅覆盖 Recording 逻辑场景和合成事件，完整 Gallery 与平台验收另行完成。
