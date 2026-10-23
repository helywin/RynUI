# Alert、Spin 与 Progress

这三个原生组件采用 typed Props、typed slots 与响应式 `Prop<T>`。普通属性变化保留节点和 slot 内容，`LayoutStyle` 仅控制外部布局。颜色和内部尺寸从 `ThemeConfig::alert/spin/progress` 读取，不扩展 CPU scene v3 或 GPU ABI v2。

```cpp
ryn::Signal<bool> loading{false};
ryn::Signal<bool> visible{true};
ryn::Signal<float> percent{35};

ryn::Alert(ryn::AlertProps{}
    .status(ryn::Status::Warning)
    .message(u8"连接不稳定")
    .description(u8"请检查设备后重试")
    .showIcon()
    .visible(visible)
    .closable()
    .onClose([=] { visible.set(false); }),
    ryn::AlertSlots{.action = ryn::AlertAction{[] {
        ryn::Button(ryn::ButtonProps{}, [] { ryn::Text(u8"重试"); });
    }}});

ryn::Spin(ryn::SpinProps{}.spinning(loading).description(u8"正在读取"),
          ryn::SpinContent{[] {
              ryn::Text(u8"保留的业务内容");
          }});
ryn::Progress(ryn::ProgressProps{}.percent(percent));
ryn::Progress(ryn::ProgressProps{}.type(ryn::ProgressType::Dashboard).percent(percent));
```

## Alert

- `status(Prop<Status>)` 直接复用 Tag/Badge 的 `status_types.hpp`。Success/Warning/Error 使用对应状态色；None/Default/Processing 显示 info。
- `message`、`description` 支持 `Prop<String>` 与 UTF-8 literal。`AlertSlots::message/description` 可替代默认文本，slot 内容自行响应，优先于对应字符串。
- `showIcon` 默认 false。`icon` 可替代默认状态图标；隐藏时仍保留 slot 生命周期。
- `action` 用于按钮等交互内容；`close` 用于装饰性图标/文字，外层已有独立 pointer/Tab/Enter/Space close 操作。
- `closable` 默认 false，`visible` 默认 true。close 只触发 `onClose()` 请求；调用方写回 visible 才隐藏，隐藏后清除交互资格。无退出动画。
- description 非空或提供 description slot 时启用两层文本、较大图标和 padding。

## Spin

- `spinning` 默认 true；无内容的 `Spin(props)` 可独立显示。尺寸使用 `ControlSize::Small/Middle/Large`，默认图标 14/20/32 logical px。
- 包裹内容始终保留。加载时在其上绘制 Theme mask，再绘制 indicator/description；屏蔽内容及逻辑后代（含其浮层）的 pointer、Tab/keyboard 和文本编辑输入，并清除已有 pointer capture/焦点/编辑会话。恢复时保留各子控件自己的 disabled/eligible 值，不自动恢复旧焦点。
- 内容尺寸不因加载切换而变化；独立 Spin 关闭时自然尺寸归零。外部指定尺寸仍服从 LayoutStyle。
- 默认 `LoadingOutlined` 复用 Icon 的有限周期旋转。仅可见且 window active 且 Theme motion/运行时 motion preference 允许时产生 deadline；离屏、祖先隐藏或销毁后取消。reduced motion 显示静态图标。
- `SpinSlots::indicator` 替换默认图标，slot 挂载一次；自定义动画由该内容负责。description 字符串可响应更新。

## Progress

- `ProgressType::Line/Circle/Dashboard`，Dashboard 固定底部 75° gap。`percent(Prop<float>)` 接受有限值并截断到 0..100；NaN/Infinity 与非法枚举抛出 `std::invalid_argument`。
- `ProgressStatus::Normal/Success/Exception`。Normal 达到 100 自动完成，Exception 优先于 100；显示百分数、完成勾或错误标记。`showInfo` 默认 true。
- `ControlSize::Small/Middle/Large`：line 厚度 4/6/8，circle 直径 60/120/160；可通过对应 Theme token 覆盖。line 使用可用宽度，圆形在非正方形外框内居中。
- line 用 rounded fill；圆弧用固定 192 个采样点的 rounded fills（现有零模糊 RoundedEffect，保留祖先裁剪），保留 topology，确定进度无持续动画。圆弧为原生采样近似实现：dashboard rail 端部使用裁剪，自定义半透明前景在采样交叠处会加深；默认前景不透明。GPU/DPI 人工视觉尚未验收。

## Theme 与验证边界

三个 component config 均支持 typed tokens、可选 component algorithm/seed，以及嵌套继承。颜色和 metrics 分开订阅，纯颜色变化不测量。token identity 和 diagnostic JSON 包含三个新增对象；原字段除整体 identity 外保留。公开 token 字段见 `feedback_tokens.hpp`。

基线为 Ant Design 6.6.5，commit `4a39f54842eade4e565ab336ef6097cd7e723cdd`；alert/spin/progress style source 的 SHA256 与 `sources.lock.yaml` 一致。Context7 当前文档只用于补充语义，未升级锁定输入。

首批范围不含 Alert banner/marquee/退出动画、Spin fullscreen/delay/percent、Progress active shimmer/steps/渐变/success segment/任意 gap、平台 accessibility adapter。没有声明原生 GPU、Linux、系统 IME 或人工视觉通过；实际 headless 测试及证据位于 change 065 的 validation.md。Gallery 与全局支持状态由父任务整合。
