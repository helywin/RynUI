# Spec Delta

## Purpose

为 RynUI 原生桌面 Switch 提供可组合、可响应更新的状态文字和图标，以及可靠的焦点、回调、方向和主题反馈；应用通过稳定的 retained identity 表达开关状态，避免为了视觉或焦点能力依赖 Web API。

## ADDED Requirements

### Requirement: 状态内容保留且宽度稳定
Switch SHALL 接受独立 checked/unchecked typed slots，支持 Text、Icon 和被动布局。两种内容 MUST 各挂载一次并共同确定最小轨道宽度，只有对应状态内容参与绘制。文字和图标 MUST 在内部内容预算内裁剪；空 slot 与旧无内容调用保持兼容。嵌套交互控件 MUST 明确拒绝并完整回滚。

#### Scenario: 文字图标切换
- **WHEN** 两种状态内容宽度不同，用户反复切换 checked，并修改 slot 内文字 Signal
- **THEN** 切换不重跑 slot、不改变轨道自然宽度；内容 Signal 改变可重新测量；主题、缩放与窄约束下内容不越过轨道预算

### Requirement: 焦点引用与回调可安全使用
Switch SHALL 提供共享 SwitchRef 的 bound/focus/blur、mount-only autoFocus 和 onClick(bool)。ref MUST 只在所属线程调用，重复 live binding MUST 拒绝，销毁后 MUST 失效且允许重绑定。有效 pointer/Space 激活 SHALL 先请求 onChange 后调用 onClick；两个回调 SHALL 报告同一目标 checked，即使受控方未回写或 onChange 销毁组件。disabled/loading MUST 抑制回调。

#### Scenario: 受控及销毁
- **WHEN** 受控 Switch 激活时 onChange 不回写，或在 onChange 内销毁组件
- **THEN** onClick 仍安全报告本次目标值，未回写时 authoritative checked 保持原值，销毁释放 ref 与全部资源

### Requirement: 原生方向及主题反馈完整
Switch SHALL 提供 reactive LTR/RTL direction，镜像手柄和状态内容的逻辑位置。内容边距、手柄阴影、wave 参数 SHALL 属于 Theme Component Token，disabled/loading 透明度 SHALL 来自 opacityLoading。enabled pointer/Space 按压 SHALL 伸展手柄；disabled/loading 不得保留伸展。旧 loading 保持有效焦点的合同 SHALL 保持。

#### Scenario: 方向主题与按压
- **WHEN** Middle/Small Switch 切换方向、主题、内容边距和透明度，并执行按压、cancel、loading
- **THEN** 手柄和内容镜像正确，阴影使用共同 logical effects，色彩变化不重跑内容或无关测量，loading 安全取消按压并保持原有效焦点

### Requirement: 激活动画有限且可取消
Switch SHALL 在有效激活时绘制有限 wave，提供 reactive wave 开关；不得为外部 checked 更新产生 wave。动画 SHALL 遵守 motion/reduced motion，重复激活复用资源，disabled/loading/window inactive/destruction SHALL 取消 wave。动画结束后 MUST 没有持续 deadline 或 frame submit，所有场景资源 MUST 通过共同 stores 上传。

#### Scenario: 动画及释放
- **WHEN** 连续激活后等待动画完成，或动画中禁用、加载、失活、关闭 motion、销毁组件
- **THEN** wave 正确结束或立即取消，不泄露 effect/target/fragment；兄弟 Checkbox/Radio 和未变化文字保持身份

### Requirement: 平台证据独立记录
共同 API、逻辑、Theme、HEADLESS 构建与测试 SHALL 在一个正式平台完成；Windows 与 Linux 窗口、输入、系统字体、GPU、DPI SHALL 各自留证。目录 implemented SHALL 只在原生功能实现和共同验收后设置，并明确专属平台待办。

#### Scenario: Windows 验收
- **WHEN** Windows/MSVC 的真实窗口已完成 Debug/Release、系统 DPI 和 1/1.25/1.5/2 render scale 验收但没有 Linux 机器
- **THEN** Windows 项独立完成，Linux 项保持待办，不替代或重做已通过的共同验收
