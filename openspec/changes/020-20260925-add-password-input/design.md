# Design

## Context

`InputComponentHost` 已复用窗口级 editor store、IME session 和 clipboard；`InputDisplayState` 当前以原文字节作为场景文本，组合输入另有 committed/display 偏移映射。锁定的 Ant Design 6.6.5 Password 默认隐藏，以可点击按钮切换显示，并防止按钮按下夺走输入焦点。本 change 将 Password 作为 Input 的特化消费者，不新建编辑宿主。

## Goals / Non-Goals

**Goals:** 隐藏场景及预编辑不含原文；复杂字素的双向映射正确；切换指针不触发失焦和 IME 取消；复用 Input 的布局、主题、编辑服务及生命周期。

**Non-Goals:** 强度校验、密码管理器、hover 切换、自定义图标、多行编辑、TextArea。

## Decisions

1. **展示策略附着 Input，而非复制编辑器。** 内部 Password Props 将隐藏状态交给 Input；`InputDisplayState` 先形成 committed+composition 的逻辑文本，再按字素生成遮罩文本与边界映射。原文始终保存在 editor，不写入隐藏 TextScene。显示模式沿用原 Input 快路径。
2. **两段映射。** committed→逻辑展示负责 composition replacement；逻辑展示→遮罩展示负责字素边界。反向映射按最近的字素边界并保留 trailing 选择。selection、caret、preedit underline 均投影到遮罩字节位置。切换只改变文本和测量，不重建场景身份。
3. **切换是独立可聚焦按压消费者。** 给交互注册增加默认开启的 `focus_on_pointer` 策略；Password 切换按钮关闭该策略，保留 Tab/Space/Enter 键盘激活。指针焦点决定在路由前发生，因此不能靠按钮 target handler 恢复焦点。其他控件沿用默认行为。
4. **平台输入类型随可见性。** SDL 文本输入类型分别映射隐藏和可见密码。会话属性更新可能重启并取消组合输入；因此切换时若组合输入活跃，先保持当前会话属性，待组合结束再同步新的类型，避免用户输入丢失。
5. **剪贴板默认保密。** 隐藏模式拦截 Ctrl+C/Ctrl+X 以及相应命令路径，不修改剪贴板或选区；可见模式沿用 Input。Password 不额外记录明文到诊断或截图元数据。

## Risks / Trade-offs

- 每字素一个遮罩符号会显示长度，不提供固定宽遮罩。这个行为符合可定位光标的单行编辑需求。
- 受控 visible 外部回写可能在 IME 组合期间到达，仍需按同一延迟属性策略处理。
- `InputDisplayState` 更新保持异常安全：校验与分配在发布状态之前完成，现有非密码热路径与分配回归需保持。
- Windows 可实际验收 Win32、D3D12/DXIL 与系统输入；Linux Wayland/SDL 专属结果以后独立记录。

## Migration Plan

新增公开 API，不迁移已有 Input。规划、投影与交互实现、Windows 验收分别提交；Linux 任务保持独立未勾选，不自动 push 或 archive。
