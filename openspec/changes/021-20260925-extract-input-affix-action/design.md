# Design

## Context

Password 的 suffix 当前在 `InputComponentHost` 私有实现中创建自己的状态、BoxLayout、交互注册、空 scene fragment、焦点 handler 与 `Text` 标签。它依赖窗口服务和 typed slot 上下文，按压行为由 `PressableBehavior` 提供。后续 InputNumber 的增减操作和 Input 的 clear 操作将需要同一组合。

## Goals / Non-Goals

**Goals:** 内部附属动作只声明 label、disabled 和 activation 回调；保持按压取消、指针切换保留 Input 焦点、键盘 Enter/Space、Tab 可达、主题语义文字及清理行为。

**Non-Goals:** 新公开 API、统一 Button 与 Input 视觉合同、实现 InputNumber 或 allowClear、改变 Password 的可见性策略。

## Decisions

1. **内部 mount helper 复用现有构建上下文。** `InputAffixAction` 从当前 active `ComponentBuildContext` 挂载组件，借用 `WindowComponentServices`，并通过传入的 `Prop<String>` label、`Prop<bool>` disabled 和回调驱动行为。组件状态持有交互 ID、fragment 和 `PressableBehavior`；清理在现有资源钩子中完成。
2. **指针焦点策略保持显式。** helper 注册 `focus_on_pointer=false`，不触发 Input 失焦；它自身仍可被 Tab 聚焦并用 Enter/Space 激活。指针路由和焦点默认策略不变。
3. **视觉最小变化。** 保留原有 Text、输入框的语义颜色与 typography、左右 4dp 点击填充。helper 不新增 Button 外观、Token 入口或 scene topology。
4. **Password 退化为消费者。** Password 只把 `visible` Signal 映射到“显示/隐藏”标签和切换回调；受控回写与 IME 延迟属性策略仍在现有 Input/Password 层。

## Risks / Trade-offs

- helper 暂时只有 Password 一个已上线消费者，但抽取自已验证的具体路径；下一控件不需复制窗口资源装配。将来操作区的特定布局/图标仍由 consumer 组合，不扩张 helper。
- 回调可以同步销毁组件，指针和焦点 handler 需复制回调再调用，不能在调用后读取状态。
- 本轮没有新增 OS、GPU 或 IME 行为；Windows 验证回归现有真实窗口，Linux 依用户安排暂缓。
