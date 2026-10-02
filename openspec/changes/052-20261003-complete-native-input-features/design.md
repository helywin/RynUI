# Design

原生窗口验收发现本机 Segoe UI Variable Text / Microsoft YaHei UI 同时不覆盖 🙂 与 U+FFFD，原有默认字体链仅探测 Latin/CJK，导致 Input intrinsic measurement 失败。默认字体加载现在额外保证 U+FFFD，按需要追加显式 bundled Latin fallback；各尺寸/样式/monospace 继续复用该链。保持 FontRuntime 的显式字形合同及 renderer R8 上传合同，未覆盖字符显示替代字形，编辑原值/统计不变。

真实 GPU 读回另外发现旧命中缓存和旧字形覆盖：affix 资格改变显式 invalidate HitTest；零尺寸/offscreen Text 同步 patch retained glyph 为零 clip，并清空装饰，恢复时使用保留 scene 和 pending 更新。该路径不 shape/rasterize 或重建 glyph range，保持 CPU scene/renderer ABI 边界。平台通用回归与最终完整测试记录在 evidence/common/visibility-regressions.md。

## Context

动机见 proposal.md。InputComponentHost 已持有长期 editor、三个文字 view、选择/caret retained surfaces 和 RoundedEffect 容器；Password/Search 通过 Input 转发属性。当前 Token 只有 Outlined 色组，公共输入属性重复，TextInputPlatform 已具备用途/大小写/autocorrect，公开 API 尚未连接。清空与密码操作复用 InputAffixAction，Search 默认图标已存在但 Clear 来源及 variant 未实现。

## Goals / Non-Goals

**Goals:** 保留现有值/事件默认合同，统一共用 typed Props，原生统计和自定义图标使用现有 Text/Icon；编辑 formatter 在发布前作为单次事务，引用及回调在同步卸载和回写下安全。

**Non-Goals:** 不增加 DOM/CSS/HTML 字段、不把 LayoutStyle 变为视觉覆盖入口、不引入新依赖，不在本 change 内实现 TextArea/OTP/bidi，也不将这三个原生缺口移出后续总范围。

## Decisions

### 1. 共用 typed 属性与版本固定的变体 Token

提取 CRTP InputPropsBase，InputProps、PasswordProps、SearchProps 返回各自类型的 fluent API，保留原方法和默认值。复用现有 InputVariant，新增 InputCountOptions 等值类型；静态 formatter 单独存储，reactive 配置保持可比较。InputRef 使用共享引用状态及 owner-thread 检查，宿主安装带 component generation 的函数并在 cleanup 清空。

变体颜色在 theme/input_tokens 与 ThemeSnapshot 推导处集中计算，公开 InputTokenOverride 提供必要的 Filled/状态/焦点值；新字段加入 Theme dirty 捕获。保留现有 RoundedEffect 容量，Underlined 底边走同一逻辑 effect，Borderless keyboard outline 使用保留 focus layer。不同变体保留等高 padding，Compact seam 只发布实际可见边框，Search 默认组成 Compact 并将 variant 投影到 action Button，遵循固定 6.6.5 variants/Search 样式。不使用组件私有 GPU 通道。

备选：按变体重建组件会失去选择/IME；在组件直接读取全局 Ant seed 会绕过局部 Token，所以均不采用。

实施发现：Compact 的裁剪角落可能没有 packed effect index。RoundedEffectStore 对此类 effect 的纯 material 更新只保留最新值，不请求 compact；后续 geometry/clip/visible 变化负责重打包。否则 Search 的主题色变更会错误触发全场景 topology 更新。独立 store 合同覆盖三类不可见 effect 的 material 保留与重新出现。

### 2. 统计内容与编辑事务分离

默认单位沿用原生 scalar 合同，grapheme 策略复用 TextBoundaryMap；自定义 countStrategy、countFormatter 和 exceedFormatter 为静态函数，showCount、max、unit 为 reactive 配置。统计文本/可见性由 Signal 更新；固定 suffix 子树先挂载，隐藏时退出布局测量和绘制，clear、自定义 suffix、counter 顺序与间距明确。默认计数仅读取 committed 值，Password 默认 counter 只输出数字。

软 max 优先于硬 maxLength 的显示上限，未配置裁剪不更改值。exceedFormatter 仅处理用户候选，UTF-8 验证及硬限制在同一编辑事务内完成；受控 authoritative reconcile 和历史恢复不经过 formatter。formatter 执行前固定 owner/revision，执行后检查是否卸载或重入改变，异常不发布候选。需要在回调运行时持有编辑状态生存期并在 store 删除时使其退休，防止回调销毁产生悬空访问。

备选：onChange 后再改值会多出历史条目、重复回调及可见中间值；只裁剪 TextCommitted 会遗漏剪贴板等入口，因此不用。

### 3. 焦点、系统提示与动作共享生命周期

Ref 操作验证活跃分支和 disabled，selection 使用现有 UTF-8 byte/grapheme 合同，不移植 UTF-16 DOM 偏移。onFocus/onBlur 回调在状态完成更新后以拥有副本调用，不保留 InputState 指针。autoFocus 是首次挂载请求，disabled 初值不在之后意外抢焦点。输入提示通过 TextInputProperties 投影，在没有 composition 时刷新；Password 类型/autocorrect 强制覆盖，任何替换会话使用新 epoch。

InputAffixAction 扩展 IconSource、focusable、hover 通知，clearDisabled 与 editor disabled/readOnly 分离；保持一次内容挂载和 retained 更新。Password visibilityToggle 使用 Prop，controlled visibility 发出候选而不擅自回写，图标函数通过 bind 更新；prefix/suffix 和 clear 统一组合。Search 输入 value bridge 继续负责 accepted 值，clear 回调显式传空候选，复制回调以承受同步卸载。Web ReactNode 视觉槽映射为 typed prefix/suffix/button；动作图标映射为 051 typed IconSource/vector。

实施核对固定 6.6.5：Underlined Search action 是 Text Button，底边只属于 Input；Filled action 额外使用 neutral 普通/hover/pressed 背景。Search 在 Button 内使用私有变体投影和 Input Token 依赖，不增加公共视觉 Modifier。Small Search 的两侧共享 max(controlHeightSM, lineHeight + 2 paddingBlock + 2 border) 最小高度。Password 默认 Eye 图标修正为隐藏 EyeInvisibleOutlined/显示 EyeOutlined，toggleFocusable 默认 true，Hover 每次 enter 切换一次。Search Clear 通知空候选，与 controlled 接受值分开，loading 仅阻止提交。

Search action 的 pointer focus 条件在 PointerRouter 转移焦点前判断所属 Input 是否聚焦。InteractionRegistry 保存共享 predicate，执行后重新核对 target generation/branch；所属输入框聚焦时鼠标保留焦点/IME，其他情况维持 Button 的鼠标焦点，键盘策略独立。

### 4. 验证边界

使用 CMakePresets 的 Ninja Multi-Config 和 MSVC：windows-msvc-headless Debug/Release 完成 common 合同和完整 CTest；windows-msvc Debug/Release 完成受影响集成测试及真实 D3D12/DXIL 窗口运行。native harness 覆盖四变体、三个尺寸、Theme/status/count、清空/密码/Search、自定义图标、焦点/IME stamp、缩放、resize、popup、idle/dispose，记录截图/hash/日志。Linux 窗口/GPU/system input 单列实际机器验收，本机缺证据则保持未勾选。

## Risks / Trade-offs

- [格式化函数重入或卸载] → 同一事务前后 owner/revision 校验，退休状态、强生存期、失败回滚及专门测试。
- [清空/密码 suffix 与 count 争用布局] → 持久子树加明确顺序、零隐藏测量与窄宽度测试。
- [系统输入 hint 的 OS 差异] → 测试端口合同与本机 SDL 属性映射，记录平台是否实际提供 UI，不声称桌面自动填充。
- [Filled/Underlined 和 Compact 接缝] → 集中 Token、逻辑图元合同及实际读回，禁止仅凭源文件审核勾选。
- [API 提取影响二进制布局] → 当前工程不保证 Props 二进制 ABI；保持源代码 API，补独立头文件编译。

## Migration Plan

按 tasks.md 阶段验证并提交，旧调用保持编译；新属性未设置时与现有 Outlined 行为一致。各阶段可单独 revert，Gallery 与支持表仅在实现和验收之后更新。
