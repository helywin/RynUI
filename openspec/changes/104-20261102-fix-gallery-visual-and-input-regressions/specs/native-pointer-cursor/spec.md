## Purpose

文本输入应通过鼠标形状向使用者清楚表达当前可编辑的目标区域，逻辑控件语义与具体平台鼠标资源分离，并在目标销毁或指针离开后及时恢复正确状态。

## ADDED Requirements

### Requirement: Text editing cursor
系统 SHALL 在文本编辑区域显示I-beam，并在离开区域后恢复相应鼠标样式。
#### Scenario: Editor hover and retirement
- **WHEN** 鼠标进入Input、TextArea、Password、OTP及组合编辑器的文本区域，随后离开或组件退役
- **THEN** 平台cursor正确切换且没有旧目标资源或形状残留

