# Design

## Context

基线为 main e39df83c；73 个目录项、7 类。导航及后续共 62 项，其中 List 已废弃，部分原生功能已实现但平台验收独立。依据本地冻结 manifest/overlay、公开头、组件测试及 component-completion 清单开展；不把历史首版 completion 等同于完整合同。动机见 proposal。

## Goals / Non-Goals

目标是依目录次序持续补齐原生功能，每个组件可单独验证和提交；现有 API 尽量兼容。浏览器 DOM/CSS/React、URL 路由、浏览器安全策略及私有 Web 工具不是公开原生 API；不提供空壳替代实现，不复活 List。

## Decisions

1. 先导航，再数据录入、数据展示、反馈和其他。导航分成 Anchor/Breadcrumb、Menu/Dropdown、Pagination/Steps/Tabs 三路本地工作树；共享基础的变动由父线程集成，后续分类依据表内依赖推进。
2. 动态内容使用稳定 key 和局部 append/destroy；普通 Prop 更新不重建无关组件。受控状态只发送请求；删除活动项采用可预测邻项回退，应用显式选择仍为权威。
3. 复用 WindowComponentServices、FocusManager、PointerRouter、输入边界及 FrameRequestState；所有 deferred/异步结果持 lifetime/generation ticket。动画用单调时间和下一帧请求，hidden/reduced motion/离页停止。
4. 新 scroll target、媒体、文件及 transport 使用 typed Core 合同与显式宿主服务；平台实现只在 adapter 层。组件不直接访问 SDL、网络或 renderer，不私自增加 GPU ABI。图片/导出需要的资源扩展经共同 SceneResources/backend 合同实现并单独验证；新增依赖先锁版本、哈希和 license。
5. typed slots 与 Theme/Component Token 控制视觉，LayoutStyle 仅外部布局。响应式使用 106 共享断点；RTL/locale 是原生上下文，不复制 Web Provider 别名。
6. Gallery 只挂载当前页，复用 existing frozen legacy IDs。组件实现代理只修改自身样例与 overlay 项；共享目录生成及 Theme golden 合并由集成阶段保证，确定性 golden 必须覆盖全部字段。
7. 测试仅构建相关 target；平台通用合同在 Linux GCC HEADLESS Debug/Release 完成一次。原生 GPU/输入在 Linux 私有 headless Wayland 运行，保留 106 frame callback 和有限 action pacing；不打开用户桌面、不控制物理鼠标。Windows 用实际 MSVC presets 独立记录。
8. 每个小阶段通过后提交英文 Conventional Commit；子工作树只保留本地提交，禁止 push T3 分支。父线程合入 main 后立即安全 fetch/rebase/push 并核对远端 SHA。无 PR、不 archive。

## 组件矩阵

本表中的“当前缺口”来自冻结目录，仅是现状，不是通过证据；与平台、Web 专用有关的描述不转化为伪原生功能。实施须对照锁定 upstream commit 4a39f54842eade4e565ab336ef6097cd7e723cdd 的组件 API、demo 和类型定义细化并验证，不能静默删减表中原生缺口。

| 分类 | 组件 | 当前状态 | 本轮原生目标 / 当前缺口 |
| --- | --- | --- | --- |
| 导航 | Anchor | planned | 提供显式原生滚动目标、键控层级锚点、横纵方向、活动项跟踪、偏移与边界、点击及变化事件、固定定位与平滑滚动；不得模拟浏览器 hash 导航。 |
| 导航 | Breadcrumb | partial | 支持动态键控条目、保留富标题和图标、分隔符、内置菜单、换行与 RTL；激活通过原生事件表达，移除项清理焦点及浮层。 |
| 导航 | Dropdown | partial | 补齐 hover 及组合触发、分离按钮、自定义保留 popup、箭头/定位/溢出、开合事件来源和受控请求；支持子浮层与焦点恢复。 |
| 导航 | Menu | partial | 支持动态键控结构、Horizontal/Vertical/Inline、横向溢出菜单、纵向子菜单、折叠、主题、RTL、多选与完整键盘导航；保留未变项身份。 |
| 导航 | Pagination | partial | 补齐页大小选择、快速跳页、simple、响应式、对齐、总数与 typed item 渲染；正确处理非法输入、总数收缩和受控拒绝。 |
| 导航 | Steps | partial | 补齐动态条目、dot/inline/navigation/panel、percent/maxCount、横纵/响应式/RTL、富内容和有限动画；禁用及移除项不得继续交互。 |
| 导航 | Tabs | partial | 支持动态键控新增/删除/重排、editable-card、lazy/destroyOnHidden、四方向/RTL、富标签和 extra slots、溢出菜单及 indicator/有限动画；保留未变面板草稿并正确清理隐藏输入。 |
| 数据录入 | AutoComplete | partial | 网络异步、分组、虚拟化、动画、RTL、自定义编辑器与 popupRender；完整 Gallery/native/GPU、Linux、系统 IME 与辅助技术验收 |
| 数据录入 | Cascader | planned | 多列层级单选/多选、搜索、动态键控选项、异步加载和禁用边界。 |
| 数据录入 | Checkbox | implemented | DOM/HTML/CSS, form name/required and React-specific interfaces；Linux native acceptance remains an independent platform task |
| 数据录入 | ColorPicker | planned | typed 色值、RGB/HSB/HEX 与 alpha、面板/预设、渐变及受控交互。 |
| 数据录入 | DatePicker | partial | RangePicker, multiple/time/week/month/quarter/year modes, locale/RTL and free date formats；presets, custom cells/panels, animations and full Ant Design Web/DOM/CSS API；native GPU/Linux/system IME and assistive technology acceptance remain independent |
| 数据录入 | Form | partial | horizontal/inline layouts, nested Form, Form.List, deep field paths and automatic arbitrary-control binding；async validation, dependencies, automatic Enter submission and automatic scrolling；native GPU/Linux/system IME and assistive technology acceptance remain independent；complete Ant Design Web/DOM/CSS API |
| 数据录入 | Input | implemented | Linux native/Wayland acceptance remains an independent pending task; OS IME candidate UI is platform dependent；DOM/CSS/React refs/events and HTML autofill APIs are outside native scope |
| 数据录入 | InputNumber | partial | stringMode, custom formatter/parser, hold repeat and wheel stepping；native Windows/Linux GPU/system IME acceptance remains independent |
| 数据录入 | Mentions | planned | Unicode 编辑、触发前缀/搜索、键盘建议、异步更新与 Form。 |
| 数据录入 | Radio | implemented | Web 专用 DOM/form/name/required、React 事件和 CSS 入口不移植；Linux 原生验收独立待完成 |
| 数据录入 | Rate | partial | GPU/原生窗口与人工视觉、Linux、系统 IME 和 AT 验收 |
| 数据录入 | Select | partial | multiple/tags, grouped options and custom option rendering；virtual list, built-in asynchronous data source and interactive affix slots；complete Web Select API and DOM semantics |
| 数据录入 | Slider | implemented | Web-only DOM/CSS/portal and compatibility aliases are excluded from native scope |
| 数据录入 | Switch | implemented | DOM/HTML/CSS and React ref；Web value/defaultValue aliases；Linux native acceptance remains an independent platform task |
| 数据录入 | TimePicker | partial | RangePicker, 12-hour/millisecond modes, locale/free formats, Now/presets and changeOnScroll；animations and full Ant Design Web/DOM/CSS API；native GPU/Linux/system IME and assistive technology acceptance remain independent |
| 数据录入 | Transfer | planned | 双列表搜索、选择、方向移动、分页、禁用及动态键控数据。 |
| 数据录入 | TreeSelect | partial | 多选/checkbox、搜索、动态拓扑、异步、富节点/自定义 popup 与虚拟化；动画、RTL/Web/辅助技术以及组件 native/GPU/Linux/系统 IME 分平台验收 |
| 数据录入 | Upload | planned | 原生文件选择/拖入、受控文件列表、类型与数量限制、进度/失败/取消/重试、预览及可注入异步传输。 |
| 数据展示 | Avatar | partial | 图片加载、响应式断点尺寸；GPU/原生窗口与人工视觉、Linux、系统 IME 和 AT 验收 |
| 数据展示 | Badge | partial | Ribbon/group, number and processing animations；arbitrary text count and complete Web/DOM compatibility |
| 数据展示 | Calendar | planned | 公历年月视图、受控选择/范围、locale、单元格及 header slots。 |
| 数据展示 | Card | partial | Card.Grid/Card.Meta and built-in Tabs/tabList；asynchronous image loading, animated skeleton and arbitrary descendant rounded mask；complete Web/DOM/CSS API |
| 数据展示 | Carousel | planned | 保留面板、切换/拖动/键盘、方向、指示器、自动播放及暂停、有限动画。 |
| 数据展示 | Collapse | partial | dynamic panel topology, lazy/destroy body policies and expand/collapse animation；borderless/ghost, custom expand icon/position, RTL and complete Web aliases；native GPU/system-font/input, Linux and manual visual/IME acceptance remain independent pending |
| 数据展示 | Descriptions | partial | dynamic item topology, automatic responsive breakpoints and locale/RTL；native GPU/manual visual/Linux/system IME and assistive technology acceptance；full Ant Design Web/DOM/CSS API and HTML auto-table sizing |
| 数据展示 | Empty | partial | upstream large SVG illustration parity, image URLs/raster assets and network loading；animation, network/state services and dynamic slot topology；Web DOM/CSS/React aliases and screen-reader semantic bridge；076 Gallery runtime acceptance, component GPU/manual visual, Linux and system IME evidence are recorded independently |
| 数据展示 | Image | planned | 解码资源、占位/失败/回退、尺寸适配、预览分组与缩放/旋转/拖动。 |
| 数据展示 | List | deprecated | 对应 RynUI 稳定公开组件与交互合同 |
| 数据展示 | Listy | planned | typed 稳定键控数据、保留 item slots、方向/网格、分页/滚动及空状态。 |
| 数据展示 | Popover | partial | contextMenu trigger, animations, RTL, wireframe special cases and built-in content scrolling；native GPU/manual visual/Linux/system IME and assistive technology acceptance；full Ant Design Web/DOM/CSS API |
| 数据展示 | QRCode | planned | 标准 QR 编码、可验证内容、尺寸/色彩/纠错、状态覆盖和图像导出。 |
| 数据展示 | Segmented | partial | pill animation, dynamic option topology, RTL and complete Web aliases；platform accessibility adapter and OS screen-reader integration；native GPU/system-font/input, Linux and manual visual/IME acceptance remain independent pending |
| 数据展示 | Statistic | partial | Timer/动画/locale；GPU/原生窗口与人工视觉、Linux、系统 IME 和 AT 验收 |
| 数据展示 | Table | partial | virtualization, expandable/tree rows, filters, fixed columns and draggable scrollbars；native Windows/Linux GPU/input acceptance remains independent |
| 数据展示 | Tag | partial | close/enter animations and drag/reorder groups；complete Web/DOM compatibility and native accessibility bridge |
| 数据展示 | Timeline | partial | horizontal, dynamic topology, titleSpan, RTL and animations；complete component GPU/native/Linux/system IME/AT acceptance |
| 数据展示 | Tooltip | implemented | Web-only DOM/CSS/portal APIs are excluded from native scope |
| 数据展示 | Tour | planned | 原生目标定位、遮罩高亮、步骤导航、开合/关闭、焦点与离页清理。 |
| 数据展示 | Tree | partial | 动态拓扑、多选、strict checking、异步、虚拟化、拖放、连接线、DirectoryTree、RTL 与动画；完整 Gallery/native/GPU、Linux、系统 IME 与辅助技术验收 |
| 反馈 | Alert | partial | banner/marquee and exit animation；complete Web/DOM compatibility and native accessibility bridge |
| 反馈 | Drawer | partial | loading/Skeleton, animation, push/drag/resize and static service；extra slot, directional shadow mapping, lazy/destroyOnHidden, RTL and native accessibility bridge |
| 反馈 | Message | partial | animations, progress, pauseOnHover, global service and promise API；native GPU/manual visual/Linux/system IME and assistive technology acceptance；full Ant Design Web/DOM/CSS API |
| 反馈 | Modal | partial | static confirm/Promise services, lazy/destroyOnHidden and activation animation；drag/resize, mask blur, RTL, responsive breakpoint widths and native accessibility bridge；Web/DOM/CSS compatibility aliases |
| 反馈 | Notification | partial | animations, pauseOnHover, progress, collapsed stack and global/promise APIs；native GPU/manual visual/Linux/system IME and assistive technology acceptance；full Ant Design Web/DOM/CSS API |
| 反馈 | Popconfirm | partial | Promise/static service, hover/contextMenu triggers and arbitrary ButtonProps forwarding；activation animation, RTL and native accessibility bridge |
| 反馈 | Progress | partial | active shimmer, steps/gradient/success segment and arbitrary gap；exact vector arc, translucent sample overlap and dashboard rail endpoint refinements；native GPU/DPI visual and accessibility acceptance |
| 反馈 | Result | partial | upstream large exception SVG illustration parity and image URL/raster/network APIs；animation, network/state services and dynamic slot topology；Web DOM/CSS/React aliases and screen-reader semantic bridge；076 Gallery runtime acceptance, component GPU/manual visual, Linux and system IME evidence are recorded independently |
| 反馈 | Skeleton | partial | active/shimmer animation, SkeletonImage and SkeletonNode；native GPU/manual visual/Linux/system IME and assistive technology acceptance；network loading and full Ant Design Web/DOM/CSS API |
| 反馈 | Spin | partial | fullscreen, delay and automatic percent；complete Web/DOM compatibility and native accessibility bridge |
| 反馈 | Watermark | planned | 文字/图像重复水印、旋转/间距/偏移、主题及内容尺寸变化。 |
| 其他 | Affix | planned | 显式滚动目标、顶部/底部偏移、位置变化通知及释放恢复。 |
| 其他 | App | planned | 有作用域的消息/通知/模态服务、上下文继承、窗口隔离及销毁取消。 |
| 其他 | BorderBeam | planned | 沿边框运动的主题化效果、时长/方向/颜色和 reduced motion。 |
| 其他 | ConfigProvider | partial | 原生 locale/direction/size/disabled/theme 上下文、嵌套覆盖和动态传播。 |
| 其他 | Util | planned | 说明上游内部工具与原生公共能力的对应关系；无独立公开 UI 合同的工具不得伪造空组件或虚构 implemented。 |

## Risks / Trade-offs

- 动态结构与输入生命周期交叉 → 测试删除活动项、富子控件、回调销毁、异常、隐藏恢复及子浮层，不依赖单纯静态截图。
- 分类并行的共享文件冲突 → 三路独立工作树，父线程逐个集成，重跑共享合同和受影响组合，保留所有新增 Theme 字段。
- 异步与媒体扩展跨度大 → 依赖顺序拆小阶段，每阶段必须有真实实现和失败/取消测试；不可用 stub 或目录改名冒充完成。
- 高事件频率和长页面 → 保持输入先分发与已有几何优先 hit-test，测真实输入路径，不在每个 wheel 上重建样例。

## Migration Plan

保持旧调用编译，新增 overload 和 typed 属性；不兼容变更须单独说明。按组件提交支持小步 git revert，避免回退用户工作。平台证据分别提交；未验证 Windows 保持未勾选。人工体验仅可选说明，不建立 checkbox 或实施前置门槛。

## Form typed API 映射

固定对照 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 Form/API、FormItem/FormList/context/useForm 类型和 basic、dynamic-form-items、complex-form-control、validate-trigger、form-dependencies demo。源码副本仅在忽略的 out。

旧 String 字段名继续是平面字面身份；显式 FormNamePath 组合字符串/数组下标。FormValue 增加 SelectValues 与 optional<Decimal>，保留旧 variant 入口及结果成员。NamePath/structuredValues、FormList 稳定 key/局部 append-destroy、FormProvider 隔离及 FormPresentation 原生布局用于对应上游原生合同。

FormItem 的首个兼容 typed 内置控件可自动绑定；显式已有 value/onChange 保留并占用该位置。自定义复合控件通过 FormControlAdapter<T> 声明 value/event/status/gate/ref/geometry，不使用 runtime reflection、React clone 或向无关后代注入 props。Web valuePropName/getValueFromEvent/normalize 对应此 typed adapter。FormRef/watch/useFormStatus 和 retained slots 对应 instance/hook/shouldUpdate 能力；不引入 VDOM。

同步规则保持旧 validate/submit 合同；异步规则增加 validateAsync/submitAsync、owner-thread posting/cancellation、每字段及每 Form 代际、快照和有界超时。FormScrollTarget 是显式原生服务，使用捕获 control 的实际几何，reveal acknowledgement 后等下一次几何同步再检查焦点资格。平台分类验收仍由 63.2/64.2 独立追踪，不用 HEADLESS 代替。

Form 富 label/help/extra/feedback 使用 typed slots，默认反馈是实际 Icon。watch/useWatch 和 useFormStatus 返回可读取 Binding，支持 Prop 与细粒度订阅；FormRef 挂载前订阅及重绑保留有效观察。getFieldInstance 映射为带注册代际的 FormControlTarget；getFieldsError 与 Provider.changed_fields 保留 typed meta。Form/Item.preserve 与 clearOnDestroy 对应内部 store 保留语义。异步工厂、取消回调、受控 getter/setter 和元数据观察者均可重入；所有变更后检查注册与事务权威，无法安全继续的应用批次退役完整 Form UI，行 renderer 失败退役完整 List，保持其他组件存活。

结构化 store 同时保留尚未挂载的 typed 路径；getFieldsValue(true)、watch、后续挂载和部分/完整 reset 使用该 store。List 自身是注册的数组值，未挂载子路径随稳定行 move/remove/reset 迁移。删除只有异步规则而没有叶字段的 List 也立即取消 Form 事务，不能依赖之后的结果或超时清理 pending。
