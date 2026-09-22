# Spec Delta

## Purpose

为多个公开交互控件提供共享的窗口输入、焦点、按压、动画与 retained scene 行为合同，使新增控件能够复用既有设施，同时保持 Button 和 Input 的现有可观察行为与最小更新范围。

## ADDED Requirements

### Requirement: 同一窗口的控件共享输入与场景生命周期
系统 SHALL 让同一窗口内的 Button、Input 与新增选中型控件使用同一套 generation-checked 焦点、指针路由、命中测试、动画时钟和 retained scene 生命周期。普通属性变化 MUST 不重新执行无关组件 closure；销毁或复用 identity 后的迟到事件 MUST 不影响新组件。

#### Scenario: 混合控件的焦点与销毁
- **WHEN** Button、Input、Switch 和 Checkbox 在同一窗口挂载，其中一个有焦点的控件被条件卸载，随后原 identity slot 被复用
- **THEN** 后续 Tab、pointer 或 animation event 不到达已卸载控件，也不错误地激活复用 slot；其余控件的 scene identity 保持稳定

### Requirement: 按压行为可复用而激活策略保持控件专属
具有按压语义的控件 SHALL 共享主 pointer down、capture、bounds 内 release、cancel、失焦、disabled 与销毁的安全收口规则。Button click、Switch toggle 和 Checkbox toggle SHALL 各自只在有效完整手势后执行一次；键盘触发键及回调含义 MUST 由控件自身合同决定。

#### Scenario: 拖出与取消不会误触发
- **WHEN** 任一可按压控件在主 pointer down 后被拖到有效 bounds 外 release，或在 capture 期间收到 cancel、失焦、disabled 或销毁
- **THEN** capture 与 pressed 被清除，不执行 click/toggle，且不会把旧 pointer 的 release 解释为新控件的激活

#### Scenario: 回调销毁自身
- **WHEN** 有效 release 触发的回调同步销毁当前控件或其父 Scope
- **THEN** pressed 与 capture 在回调前收口，回调仅执行一次，返回后不访问已销毁状态或 scene range

### Requirement: 共享机制保持最小失效和公开边界
跨控件共享 SHALL 不引入稳定公开控件基类、通用视觉 Modifier、SDL3/GPU 类型或新的第三方运行时依赖。纯 hover、pressed、checked、focus 或颜色更新 MUST 只触及必要的 material/effect/interaction range；无动画及无输入时 MUST 恢复 idle。

#### Scenario: 单个控件状态更新
- **WHEN** 混合控件树中只有一个 Switch 的 checked 值改变且尺寸与内容不变
- **THEN** Button、Input、Checkbox 的 closure、measure、scene identity 与上传 range 不变，系统只提交必要的 Switch 视觉更新并在完成后恢复 idle

#### Scenario: 既有组件回归
- **WHEN** 内部共享机制接管 Button 与 Input 的同步阶段后运行既有 pointer、keyboard、IME、scene、Theme 与 benchmark 合同
- **THEN** 两个公开 API、交互结果、视觉状态、平台服务所有权和稳态性能边界保持不变
