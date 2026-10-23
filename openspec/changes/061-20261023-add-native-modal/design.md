# Design

## Context

动机见 proposal。WindowComponentServices 拥有共享 scene、Input/Focus/Pointer 与各组件 host，正常键盘和滚轮分发均逆序遍历 participants。已有 window layer 保留逻辑 parent/Theme，clip_boundary 阻止外部滚动再次平移弹层。Select/Tooltip 提供非模态层；FocusManager 的顺序依赖 eligible、tab_stop 和 branch_active，支持 defer_focus，Escape 会先让编辑器处理组合事务。当前没有模态输入边界。

已读取 Context7 官方 Modal 文档，并与本地固定6.6.5 `components/modal/index.en-US.md`、`style/index.ts` 对照。默认保留内容、受控关闭、宽520、top100、mask/keyboard/closable、confirmLoading 的语义适用；React portal、Promise 静态服务、Web样式不作为原生实现。

## Goals / Non-Goals

**Goals:** 用已有窗口层和最小输入边界支持普通确认及编辑对话框；复用真实 Button/Input/Select 与 retained 内容，正确处理嵌套、回调卸载、裁剪和滚动。外部布局占位不占正文空间，背景可见但不可交互。

**Non-Goals:** 不建立全局对话框服务、异步任务框架或新渲染后端。slots 首次一次预建，无 lazy/destroyOnHidden；无动画、静态 confirm、拖动/resize、mask blur、平台可访问性桥接、RTL、响应断点或 Web别名。后续 Drawer/Popconfirm 可按实际需要复用内部合同，本次不预设它们的抽象。

## Decisions

### 1. typed 受控 API 和生命周期

新增 ModalProps：open（默认false）、title、width（默认520 logical units）、centered、closable、keyboard、typed ModalMask（enabled/closable）、confirmLoading、okText/cancelText、footer mode（Default/None）、restoreFocusAfterClose，以及 onOk/onCancel（typed reason）、afterClose/afterOpenChange。ModalContent 必需，ModalSlots 可选标题/页脚；自定义页脚优先于默认模式。默认文字 OK/Cancel 可响应替换。

open 始终由应用持有；OK/取消只通知，不做本地乐观关闭。confirmLoading 只阻止 OK，保留取消。初始关闭不通知关闭；转换通知在内部显示/隐藏和输入清理完成后发布，复制回调并检查 owner generation/scope，防止回调卸载后继续访问。不开 Promise/静态服务入口，避免混合两套关闭合同。

### 2. 保留的 overlay 子树

window-owned Modal host 为正常流挂载零占位 owner，并一次挂载屏幕 shield、对话框、header/body/footer、默认按钮和 slots。实际弹层使用已有 window layer；隐藏分支在挂载内容前关闭，防止 Input.autoFocus 提前启动。显示/隐藏只改 branch activity 与交互边界，正文 Input 草稿/选择值/组件身份保留。

对话框自身提供无子入口时的焦点回退；有可用子入口时它不额外增加 Tab stop。默认 close 和页脚 Button 复用主题/加载/键盘合同。布局和绘制使用 Constraints/既有圆角、阴影、clip 和 opacity，不加GPU字段或shader。

### 3. 模态输入边界不改背景 disabled

在内部 InteractionRegistry 中记录可选的模态逻辑根，将有效交互判定与组件绘制活动区分。无边界时完全沿用原行为；有边界时 pointer hit/capture、FocusManager 与引用聚焦、输入会话只接受该根及后代。对其他 host 的滚轮/延时弹层判定补相同的组件输入有效性，背景仍绘制，不逐项覆盖 eligible/disabled。

Modal host 维护活跃对话框的打开顺序与失效检查，最上层为唯一交互边界；嵌套关闭恢复下一层。边界变化清理外部捕获/焦点/输入及 Select/Tooltip，保留父级控制值并抑制旧外部层再次冒出。处理 change/close/owner destroy/window inactive 时的 generation 与回调重入，不在 noexcept 析构通知内调用不受保护的用户回调；聚焦使用现有 defer_focus 和安全同步点。

替代方案是暂时把所有背景控件 disabled，或仅依靠遮罩命中。前者覆盖应用状态并漏掉新挂载控件，后者漏掉键盘/引用聚焦和滚轮，均不采用。

### 4. 子控件优先与焦点顺序

Modal participant 注册在 Tooltip/Select/Tabs 与外部 Input host 之前，使现有逆序输入分发先处理编辑器与子弹层，Modal 再处理未消费的 Escape/正文滚轮。背景 host 的输入有效性加入边界检查。Escape 继续保持 FocusManager 的 IME 优先逻辑。

打开记录有效触发 interaction；先聚焦正文可用入口，之后默认 OK、其他入口、对话框。Tab 顺序复用过滤后的 FocusManager 环绕，支持子 Tabs/Radio 的既有 roving tab stop。关闭恢复仍有效/未禁用/可见且在新边界内的触发入口；无效则安全降级到下一 Modal 或无焦点。用户可关闭自动恢复。restore 不恢复旧输入组合事务。

### 5. 视口适配和正文滚动

对话框水平居中；非centered优先top100，视口不足时夹紧到安全边距。width必须正且有限。标题/close和页脚先测量，正文限于剩余高度，长内容垂直滚动；极小视口安全裁剪，尺寸非负。body 保留自然内容高度，滚轮和焦点/caret 显露只改变像素对齐平移/clip；不重新放置全部字形，不平移 clip_boundary 的子窗口层。Select/Tooltip 后续按锚点定位，因此跟随滚动并夹紧到窗口。

### 6. Token 与性能边界

新增 Modal 独立 colors/metrics/typography/effects/层级 dirty domains，沿用 ThemeSnapshot/config/runtime/hash/序列化/golden与覆盖验证。配方使用固定6.6.5的 elevated背景、heading颜色/16px标题、strong字重、20垂直/24水平padding、header gap8/footer gap12、圆角8、close32与boxShadow；mask复用alias遮罩色，layer以popup base为基准并处理嵌套和子层顺序。

背景/文字纯色只更新材料；字体和尺寸才测量/布局。滚动只平移和clip patch，首次显露可实现新字形；无动画意味着idle无持续截止时间。仅为本次 Modal 增加必要状态和边界，不抽象为通用dialog框架。

### 7. 验收与兼容性

正式构建使用 Ninja Multi-Config 与 CMakePresets。共同 API/model/token、交互边界和真实组件测试使用实际 Windows/MSVC x64的 windows-msvc-headless Debug/Release；完整 CTest只在这里各跑一次。Gallery Recording 合同使用 windows-msvc。Windows窗口验收独立记录D3D12/DXIL、字体与system/1/1.25/1.5/2缩放、Light/Dark、PNG/log/EXE hash；Linux native Vulkan/SPIR-V 另列，缺少环境不影响共同/Windows已完成项。自动注入composition不代替人工系统IME。

## Risks / Trade-offs

- [输入边界漏到控件自身滚轮/延时浮层] → 逐一覆盖 Input/Select/Tooltip/Tabs，并用模态前后真实交互回归证明背景没有被修改。
- [嵌套层级与旧弹层遮挡] → 以打开顺序确定最上层和子层位置；打开/关闭/父级卸载均检查逻辑归属，不只依赖固定zIndex。
- [回调卸载和延迟焦点重入] → 拷贝通知、generation/scope复核、捕获取消和延迟焦点；覆盖 onCancel/onBlur/afterClose 中关闭、重开、卸载。
- [预建内容增加初始化成本] → 首版明确固定 slots，保持显示和滚动不重跑；后续按需求另做懒加载，不引入本次用不到的动态结构。
- [极小窗口无法完整显示所有区域] → 维持有限非负布局、正文滚动和窗口裁剪，验证可恢复resize，不声称任意大小都能显示全部内容。

## Migration Plan

依次提交 API/model/token、输入边界、Modal host、Gallery、各平台证据。新API为增加式；边界未启用时现有控件行为保持。阶段提交可单独回退；未验证任务不勾选，不自动archive。
