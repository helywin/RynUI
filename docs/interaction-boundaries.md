# 内部模态输入边界

061 引入窗口内输入限制；107 为原生 Tour 增加独立 owner 约束和 Modal controls 授权。这里描述共享内部合同；Tour 的组件/Gallery 完成状态仍以其 tasks 和证据为准。

`InteractionRegistry::set_input_boundary(optional<ComponentId>)` 和 `set_input_blocked(root,bool)` 保留独立的 legacy lane。重复 true 仍幂等，不是引用计数，也不能用一个调用者的 false 释放其他 owned lease。旧调用者若共享同一个 bool 位仍不能获得独立所有权；需要独立语义时使用 owned API，不把旧接口伪装成多 owner 计数。

`input_root` 产生携带 weak window 身份、owner thread 和 exact ComponentId generation 的能力；不能把另一个窗口恰好相同的 local ID 当作本窗口 root。`create_input_constraint` 创建 inactive lease，窗口服务先为 exact owner scope 安装一次 quiet cleanup，然后才发布策略。每次重开不累积 cleanup。owner scope 退休会精确释放其 lease 和关联 grant；保留 state storage 不延长输入权限，也不清理无关 Registry/legacy 位。过期 handle 不能释放同 slot 的新 generation。

一条 `RestrictToSubtrees` 内部的 roots 取并集，多条 restriction 求交；legacy block、owned block、disabled/eligible、隐藏分支和窗口 activity 仍优先。有效但空的允许域拒绝全部，target 退休只失去该分支，不自动放开背景。`InputConstraintBatch` 在暂存副本中校验、分配并原子 commit；未 commit、校验失败或分配失败保持旧策略，Registry mutation 不调用应用代码。热命中不分配，不枚举背景补集，仍先判断 committed geometry/clip 再检查 live 分支和约束。

Modal 只发布既有 opening-stack 规则选中的有效域，不把所有 sibling 对话框盲目求交。`ModalComponentHost::effective_input_domain()` 返回实际选中的 Modal 根；`input_boundary()` 为旧调用者保留 legacy/有效 Modal 代表根查询，**不能代表完整复合允许域**。实际接收资格使用 `component_active`；维护合法后代使用 `subtree_intersects_input_domain`，后者不会授权 ancestor 或 sibling 接收输入。Image/Upload 可维持合法 child popup，FloatButton 结束自身失效 gesture 时不取消仍被允许的后代 capture。

原生 Tour controls 通过 Modal 的内部 `register_tour_controls` 协议登记 exact owner/controls；grant 仅扩展该 authority 的域，并与 Tour 自己的 restriction 及所有其他 gates 求交。controls 必须属于 grantee，不能传入窗口共同祖先。`ModalInputCompanion::update` 把 grantee 自己的约束变化和 controls grant 一次发布，再协调用户回调；scope/Modal epoch/controls generation 退休均撤销授权。child Modal 在 controls 子树内时不授权其祖先 controls，保护 innermost trap。产品决定为保留 Modal 限制：真正处于 Modal 外的 Tour target 暂不可交互/不可用，不随 controls 获得越界授权。未关联的并行 masked Tours 不自动合并允许域。

正常发布通过 WindowComponentServices：使 HitTest 失效、停止不合法文字 session、取消外部 pointer，再以既有 sanitize/defer 机制同步焦点并通知组件。键盘 callback 内同样不能投递新 preedit/text/candidates；窗口内部 editor→exact component/interaction 表提供无应用/Form predicate 的 admission adapter，session start 和 recipient 每次检查当前许可及原有 epoch/window/disabled/read_only。约束不写入 editor.disabled。activation_allowed 回调前后和最终 activate 复核最新资格；scope 退休立即拒绝普通输入；必需的 cancel/leave 清理事件仍按旧合同交付，显式 quiet retirement 使用原有非投递路径。普通 gate 变化保留合法 blur/cancel/leave。

回调改变 policy revision 或 participant registration epoch 时终止旧通知轮次；detach/attach 即使复用地址也进入新代际。回调抛出不会把未完成 revision 永久标记为已协调，安全点可重试；不收敛变更在 32 次发布后明确失败，不产生无限轮询。窗口 command/wheel/pointer 的旧 callback 轮次也在 registration 变化后终止。焦点恢复使用 `focus_request_revision` 与 `defer_focus_if_current`：精确 InteractionId/window 仍合法且没有更新的显式/延后请求时才排入恢复；blur callback 的新 focus/clear 请求也有优先权。quiet retirement 不调用任意用户代码，之后由正常窗口服务协调；稳定等值更新不请求帧或 deadline。

Select会关闭并抑制已显示的外部菜单，不改应用的open值；Tooltip清理外部hover/action/deadline并限制显示，受控open保持应用权威。Tooltip关闭只取消其自身popup后代的指针，焦点交给安全同步点，避免清理无关编辑器。Slider在边界切换时取消外部gesture/capture及hover，包括在onChange回调内打开边界的情况。Button/Input/选择控件/Slider等ref在边界外返回false。

滚轮先依据顶层命中尝试TextArea，未消费再按既有participants逆序处理Select/Tabs和容器。菜单覆盖编辑器时，顶层命中自然排除下方编辑器；编辑器边缘的未消费滚轮仍可向父容器传递。存在要求消费未处理滚轮的有效 owned confinement 或 legacy 边界时，窗口服务最终消费未处理滚轮，防止外部Gallery文档滚动。无边界时保持既有外部滚动回退。

边界变化允许一次按现存interaction/participant扫描和临时身份快照，防止回调卸载使迭代失效；等值边界直接返回，稳定帧和普通指针查询不新增分配或定时器。不引入SDL或renderer类型，也不改变场景GPU ABI。

Modal使用正常流外的保留owner和独立overlay。内部Node的out_of_flow只用于让Box/Flex/Space及顶层正文忽略该owner的尺寸和gap；overlay仍保留逻辑祖先及Theme。窗口层可记录Modal打开顺序，子窗口层继承最近Modal组；活跃Modal组使用其中最高的z-index作为全局base，再按打开顺序排列整组；组内先画组根，再按局部priority绘制子层。使用元组排序，不对z-index相加，极值Token不会溢出或把子菜单压在对话框下面。命中同时采用Node内容裁剪；经过独立clip_boundary时重置交互父clip，使子浮层能越过正文和对话框。

窗口层使用整个窗口作为clip基准，普通组件保留调用者的文档clip，再叠加组件祖先的内容裁剪。Input、Button、选择控件、Card、Tabs、Typography、Divider和SpaceAddon复用WindowComponentServices::component_clip，默认页脚的focus可使用对话框padding。模态显示期间全局命中覆盖整个窗口，视口或clip改变时更新命中快照；关闭后恢复文档clip。

Quad没有逐实例clip。仅在活跃模态层的裁剪祖先下，RetainedSurface把已有quad数据映射到既有四角fill effects，保持源quad索引和阴影前后顺序；focus/阴影也叠加相同的祖先裁剪。不改CPU scene v3或GPU ABI v2，不加shader字段。fill身份首次显示后保留，后续材料/平移/clip更新复用它们，owner销毁时释放；无Modal时跳过该扫描。正文中的Button/Slider/选择控件可正确裁剪，不覆盖固定标题或页脚。

Modal显示转换先切换活动分支、维护打开栈、协调边界/捕获/焦点，再在窗口prepare阶段通知生命周期。清理时只在on_destroy选择仍存活边界和恢复候选，用户回调交给窗口服务的安全协调点。新焦点使用defer_focus；超高编辑器的新caret在Input geometry发布后采样，若超出正文，只申请一次后续平移帧，无Modal轮询定时器。插入点显露依据文本排版revision、caret byte及正文尺寸变化；外部平移或编辑器自身滚轮不改变这些语义，因此手工滚轮不会被未变化的焦点撤销。
