# Rate 原生评分

`Rate` 使用 retained Icon/Text、原生指针和键盘服务，不引入 DOM/CSS 或组件专有 GPU 上传。

```cpp
auto rating = ryn::state(2.5F);
ryn::Rate(ryn::RateProps{}.value(rating).allowHalf()
              .onChange([rating](float value) { rating.set(value); })
              .tooltips({ryn::String{u8"差"}, ryn::String{u8"一般"}, ryn::String{u8"好"},
                         ryn::String{u8"很好"}, ryn::String{u8"极好"}}));
```

默认 count=5、defaultValue=0、allowHalf=false、allowClear=true、keyboard=true、size=Middle。
`value` 与 `defaultValue` 互斥。有限值限制到 0..count，再按 .5 或 1 四舍五入；NaN/Infinity 拒绝。
count 是 reactive `Prop<int>`，范围 0..1000；缩减只隐藏已建项，恢复保留身份。
count/allowHalf 变化重新规范化原始权威输入，不触发 onChange；非受控的用户提交保存新的原始值。
受控 onChange 提供候选，只有应用回写 value 才改变权威显示，拒绝回写不留下提交后的 hover 假象。

`onHoverChange(std::optional<float>)` 表示鼠标预览，离开/提交/取消/输入阻断发送 `nullopt`。
Left/Right 每次增减一级（allowHalf 时 .5），Home 到 0，End 到 count；Enter/Space 提交当前位置并应用 allowClear。
每个 Rate 只有一个 Tab 入口。disabled/readOnly 禁止自身编辑；readOnly 不建立后代输入封锁。
visible=false 和 count=0 不保留布局 flow/gap。隐藏、窗口失活或 Modal/Spin gate 会取消临时输入状态。

`character(RateCharacter{u8"好"})`、`character(RateCharacter{IconName::HeartFilled})` 或纯值 index 函数可定制字符。
RateCharacter 只接受可复制的绘制值（String/IconSource），不会重复挂载原生子控件。逐项 Tooltip 使用已有原生 Tooltip。
`tooltips` 接受 `RateTooltip` 列表；条目可由文本/`Prop<String>` 构造，或用 `RateTooltip{TooltipTitle{...}}` 提供 typed 富标题 slot。
每个富标题只挂载一次，count/隐藏/主题变化保留其原生控件、编辑草稿和 ref；readOnly 仅关闭评分自身编辑，仍可操作标题中的原生控件。
外部布局用 LayoutStyle；`ThemeConfig::rate.tokens` 控制 star/background、三种 star_size、gap、hover_scale、focus_width、font_family。
锁定 Ant Design 6.6.5 的 starSize=controlHeight×.625、marginXS、yellow6、colorFillContent；默认 hover scale=1.1。
所有最终字体至少 .5 logical dp，主题发布前验证独立 seed、继承与祖先组合，失败不发布部分快照。

验证范围与精确命令见 change `090-20261026-add-native-rate` 的 evidence。HEADLESS 逻辑场景和合成原生事件不代表真实 GPU/原生窗口、Linux、系统 IME 或 AT 已验收。
