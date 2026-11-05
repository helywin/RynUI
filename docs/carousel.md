# Carousel 原生合同

设计基线为 [Ant Design 6.6.5 Carousel](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/carousel/index.tsx)，固定提交 `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 API、styles 与 demos。公开入口为 `<ryn/carousel.hpp>`；可编译示例见 `examples/carousel_example.hpp`。Carousel 使用现有 Core logical scene、布局、输入和窗口时钟服务。

| 上游语义 | C++ 映射 |
| --- | --- |
| children / initialSlide | `slides(Prop<vector<CarouselSlide>>)` / `defaultCurrent`（`initialSlide` 别名）；每项非空唯一 key 与 typed `CarouselContent` |
| slickGoTo / ref.goTo / next / prev | 响应 `current` 与 `CarouselRef::goTo(index, dont_animate)` / `next` / `prev`；bool 返回是否实际接受 |
| beforeChange / afterChange | 接受后的旧/新 index 与完成 index；每个实际切换各一次；初始挂载不派发 |
| effect / speed / cssEase | `CarouselEffect::ScrollX/Fade`、`Duration`、typed `CubicBezier`；实际平移/透明度输出 |
| infinite / adaptiveHeight / waitForAnimate | 同名 typed 响应 Props；有限边界、保留面板高度插值、动画期间拒绝交互请求 |
| arrows / prevArrow / nextArrow | 实际原生 Button 或 typed `CarouselArrow` 工厂，绑定 next/disabled 元数据与激活动作 |
| dots / customPaging | 实际原生 Button 或 typed `CarouselDot` 工厂，绑定 key/index/active/progress 与激活动作 |
| dotPlacement / vertical / rtl | Top/Bottom/Start/End；未显式 vertical 时 Start/End 推导纵向；`direction(TextDirection)` |
| autoplay / autoplaySpeed / dotDuration | 响应开关、单调间隔、显式进度；Ref `play`/`pause`；hover/focus/隐藏/失活/reduced motion 暂停 |
| draggable / swipe / touchThreshold | 鼠标背景拖动、触摸滑动、extent/threshold 的接受阈值；capture/cancel 与有限边界 |
| style / Component Token | 外部尺寸使用 `LayoutStyle`；视觉使用独立 `CarouselThemeConfig`，含 scoped seed/algorithm/inherit |

C++ 增加受控 `current` 和 `onCurrentChange(index, source)`：请求回调可同步写回接受；拒绝时不改变当前项、不调用 before/after。外部属性更新是权威状态，动画中的外部更新会先结束已接受切换。before/after 中的 Ref 重入拒绝，回调中的权威属性更新在下一次窗口同步接受。回调异常使本 Carousel 退休并释放输入/动画资源；其余组件保留。Ref 使用线程与 generation 检查，卸载后返回 false。

同一个 `CarouselSlide` 描述符的 key/content 身份不变时保留子树；重排不重建草稿或派发切换。替换同 key 的 content 描述符只替换这一项；删除当前项在旧位置附近选择仍存在项。受控列表变化按有效范围夹紧 index。追加内容先构建，失败时清理新分支并保留旧内容。无限首尾切换只移动已有面板，不复制有状态子树。

ScrollX 在逻辑裁剪 viewport 中平移已有面板；Fade 使用共同 retained subtree paint opacity。绘制过渡与输入权限分开：仅实际可见的已接受当前项可交互，旧项、预览项和屏外项的焦点、capture、Core preedit、浮层动作会退休，草稿仍保留。父级只从背景开始拖动，Input/TextArea 的选择、编辑器键盘与嵌套 Popover 使用各自输入路由。root focus 支持方向键、Home/End；横向 RTL 反转方向键。

自动播放按单调 deadline 工作，暂停保留剩余间隔，手动切换重置间隔；Ref play/pause 是显式覆盖。没有进度需求时只在到期唤醒；dotDuration 才在播放期间请求有限进度帧。暂停、空列表、非无限末项、隐藏和 reduced motion 不持续请求帧。失活、resize 和 reduced motion 结束已接受的有限过渡，完成回调仍只派发一次。下一帧 deadline 与本帧 dirty invalidation 独立。

上游 React/DOM/CSS 的 className、prefixCls、nativeElement、innerSlider、HTML attributes 和浏览器 portal 不属于 C++ API。react-slick 的多列网格、center mode、variable width、asNavFor 与浏览器 responsive/settings 插件不是本次单 viewport 原生 Carousel 合同；使用原生布局组合面板内容。原生资源可以通过 typed content 组合，Carousel 不实现 URL/HTML、图像解码或私有 GPU 上传。实际系统 IME、OS 辅助技术桥接、Windows/MSVC 与 GPU/桌面验收须以对应平台结果为准，HEADLESS 输入与场景测试不代表这些平台已实测。
