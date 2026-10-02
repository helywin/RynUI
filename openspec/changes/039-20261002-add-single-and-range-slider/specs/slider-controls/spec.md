## Purpose

为 RynUI 的 Data Entry 提供可复用的单值与双端范围选择合同，使应用可以通过强类型响应式属性控制数值，并在共同输入、主题、布局和 retained scene 上得到一致的交互行为。

## ADDED Requirements

### Requirement: Typed values and limits

系统 SHALL 提供独立单值与范围 Props，value 与 defaultValue 互斥，limits 原子响应式更新。数值 MUST 有限，minimum < maximum，step 为正且可表示；范围端点按升序归一化。数值 SHALL clamp 到闭区间并选择最近 step 点或 maximum，等距时取较大值。

#### Scenario: Controlled and uncontrolled
- **WHEN** 用户调整 controlled Slider 而调用者尚未回写 value
- **THEN** 触发候选值回调，显示仍由 value 决定；uncontrolled Slider 自行保留新值，外部 value/limits 归一化不触发用户回调

#### Scenario: Invalid and fractional values
- **WHEN** 输入 NaN/无穷/非法 limits，或者设置小数 step 的有效数值
- **THEN** 非法输入在修改组件前拒绝；有效数值稳定归一化，不通过整数强制转换截断

### Requirement: Pointer capture and completion

系统 SHALL 支持 primary mouse 与 touch 的轨道点击和 thumb 拖动，捕获期间越出轨道仍 clamp 更新；范围选择就近端点且端点不跨越。onChange 仅在候选数值变化时触发，成功释放后 onChangeComplete 触发一次；cancel、禁用、窗口失焦或销毁 MUST 清理拖动并不产生完成回调。

#### Scenario: Range and cancellation
- **WHEN** 拖动下端点越过上端点，再发生 pointer cancel 或 disabled
- **THEN** 下端点停在上端点，capture 被清理，取消不触发完成回调，之后可以开始新拖动

### Requirement: Focus and keyboard

系统 SHALL 为每个 thumb 提供独立 Tab 焦点及可见焦点效果。方向键按一个 step 调整，Home/End 到边界，PageUp/PageDown 按十个 step 调整；vertical 默认从下向上增大，reverse 翻转视觉与方向键映射。keyboard=false MUST 阻止数值键盘操作，disabled MUST 阻止全部交互并移出 Tab 顺序。有效按键 release 完成一次，repeat 不重复完成。

#### Scenario: Two focus stops
- **WHEN** Tab 遍历 RangeSlider 的两个 thumb 并用方向键调整第二个
- **THEN** 两端点焦点互相独立，只调整第二个端点，保持范围有序

### Requirement: Theme and retained logical scene

系统 SHALL 提供 Slider Component Token 及 Theme 继承、算法与覆盖。颜色变化只失效 material，尺寸变化失效布局/geometry；普通数值更新 MUST 不重执行无关 Component。轨道与 thumb SHALL 通过共同 logical scene 渲染，保留 scene 身份、clip、translation、focus 与失败恢复合同。

#### Scenario: Local updates
- **WHEN** 更新一个 Slider 的 value 或颜色 token 后同步 scene
- **THEN** sibling slot 不重执行，renderer 收到局部更新；idle 同步无新的 buffer/atlas 上传，不引入 SDL 类型到组件
