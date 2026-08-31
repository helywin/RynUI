## Purpose

为 RynUI 单行文本控件提供平台无关、Unicode 安全且可确定性验证的编辑状态、IME composition、selection、clipboard 与 undo/redo 行为合同。

## ADDED Requirements

### Requirement: 编辑内容和位置必须保持有效 Unicode 边界
文本编辑状态 SHALL 只保存有效 UTF-8，caret、selection、delete 与 replace 操作 MUST 落在可呈现文本 cluster 边界，不得拆分 combining sequence、emoji sequence 或多字节编码单元。来自平台的无效 UTF-8 或越界 composition range MUST 被拒绝并记录诊断，且不得部分修改当前 value。

#### Scenario: 删除组合字符
- **WHEN** value 包含由多个 Unicode scalar 构成的一个可呈现 cluster，用户在其后执行 Backspace
- **THEN** 编辑状态 SHALL 一次删除完整 cluster，并把 caret 放到前一个合法边界

#### Scenario: 拒绝无效平台文本
- **WHEN** 平台 text event 携带无效 UTF-8 或无法映射到当前 composition 的 range
- **THEN** 编辑状态 MUST 保持 value、selection 与 history 不变，并增加可观测 error diagnostic

### Requirement: 基础编辑命令必须具有确定的 selection 语义
编辑状态 SHALL 支持 insert、replace selection、Backspace、Delete、Left/Right、Home/End、Shift 扩展、Select All 与 pointer placement；无 Shift 的移动 SHALL collapse 现有 selection，disabled 或 read-only owner MUST 拒绝会改变 value 的命令但仍允许 copy 与 selection navigation。

#### Scenario: 输入替换 selection
- **WHEN** 非空 selection 存在且收到 committed text
- **THEN** 系统 SHALL 以 committed text 原子替换 selection，并把 caret 放到插入内容之后

#### Scenario: read-only copy
- **WHEN** read-only owner 中存在 selection，用户执行 copy 后再执行 cut
- **THEN** copy SHALL 提供选中文本，cut MUST 不改变 value 或 selection

### Requirement: IME composition 必须与 committed value 分离
系统 SHALL 把 composition text、composition selection 与 candidate metadata 保存为临时状态；`text editing` 更新 MUST NOT 触发 value change 或 history，只有 committed text 才能修改 value。composition commit SHALL 形成一个原子编辑 transaction，cancel/blur/destroy SHALL 清除未提交 composition。

#### Scenario: 中文 composition 提交
- **WHEN** IME 依次发送 composition 更新、candidate selection 和 committed text
- **THEN** 组件 SHALL 在 composition 期间显示临时文本及选区，commit 时只触发一次 value change 并清空 composition

#### Scenario: composition 中失焦
- **WHEN** owner 在 composition 尚未 commit 时失去 keyboard focus 或被销毁
- **THEN** 系统 MUST 停止输入会话、清除临时 composition，且不得把临时文本写入 value 或 undo history

### Requirement: 平台文本输入会话必须跟随有效 focus owner
每个 window SHALL 最多存在一个 active text input owner。owner 获得 keyboard focus 时系统 MUST 启动输入会话并提供当前 input area/caret offset；focus、layout、scroll 或 DPI 改变后 MUST 更新候选窗锚点；blur、window focus loss、disable、destroy 或 generation reuse 时 MUST 停止旧会话并拒绝 stale event。

#### Scenario: DPI 后更新候选窗位置
- **WHEN** focused input 的 logical caret 未变但 window display scale 或 viewport translation 改变
- **THEN** 平台桥 SHALL 用新的 window coordinate 更新 input area 和 caret offset，且编辑 selection 不变

#### Scenario: 销毁后收到迟到事件
- **WHEN** text input owner 被销毁并复用 slot 后收到属于旧 generation 的 text event
- **THEN** 系统 MUST 丢弃该事件，且不得改变新 owner 或重新启动旧会话

### Requirement: Clipboard 操作必须使用 UTF-8 快照并保持失败原子性
copy/cut/paste SHALL 通过平台无关 clipboard contract 交换 UTF-8 text。copy 和 cut MUST 使用发起命令时的 selection snapshot；paste MUST 在取得并验证完整 clipboard text 后再执行单个 replace transaction；平台失败或无 text 时不得改变 value、selection 或 history。

#### Scenario: 粘贴多行文本到单行 editor
- **WHEN** clipboard text 含有 CR、LF 或 CRLF 并粘贴到单行 editor
- **THEN** 系统 SHALL 移除换行 code point 后以一个 transaction 插入剩余有效 UTF-8 内容

#### Scenario: Clipboard 读取失败
- **WHEN** paste 请求无法取得平台 clipboard text
- **THEN** 系统 MUST 保持编辑状态不变并报告失败 diagnostic

### Requirement: Undo 与 Redo 必须按用户编辑 transaction 恢复完整状态
history SHALL 记录 committed value 与 selection 的有限 transaction，连续 text commit 可以按同一输入 batch 合并；selection jump、paste、cut、composition commit、external value reconcile 与显式 submit MUST 结束合并。Undo/Redo MUST 恢复对应 value 和 selection，新的编辑 MUST 清空 redo 分支，history 容量和字节预算 MUST 有明确上限。

#### Scenario: Composition 只撤销一次
- **WHEN** 多次 composition 更新最终产生一次 committed text 后执行 Undo
- **THEN** 系统 SHALL 一次恢复 composition 开始前的 value 与 selection，不逐个回放临时 composition

#### Scenario: 新编辑清除 redo
- **WHEN** Undo 后执行新的 committed edit
- **THEN** 系统 MUST 清除 redo 分支，且后续 Redo 不得恢复被分叉的 transaction

### Requirement: 受控值 reconcile 必须保留单一事实来源
当 owner 提供新的 authoritative value 时，编辑状态 SHALL 在有效 UTF-8 与 cluster 边界上 reconcile；与当前 edit revision 匹配的回显 MUST 保留可映射的 caret/selection，冲突的 external value MUST 结束 composition 与 history merge、替换 value 并把 selection clamp 到合法边界。

#### Scenario: onChange 回显
- **WHEN** owner 把最近一次 committed edit 的相同 value 作为 authoritative value 回传
- **THEN** 编辑状态 SHALL 保留该 edit 后的 caret、selection 与 undo transaction，不产生第二次 change

#### Scenario: 外部冲突更新
- **WHEN** composition 或本地编辑期间收到不同的 authoritative value
- **THEN** 系统 MUST 取消临时 composition、应用外部 value、clamp selection，并不得再次发出相同 external value 的 change callback

### Requirement: 编辑热路径必须保持 bounded retained state
预热后的 caret movement、selection update、composition update 与 history navigation SHALL 复用已保留容量，不得按 frame 增长 owner、event、scene 或 history storage；value 长度和 history 预算达到上限时 MUST fail atomically 或按已声明的 oldest-transaction eviction 规则处理。

#### Scenario: 长时间 composition 更新
- **WHEN** 同一 owner 执行一万次 composition/caret update 且 value 未增长
- **THEN** owner identity、scene topology 与 storage capacity SHALL 保持 bounded，更新不得触发无关 Component content、Measure 或 sibling HitTest
