# Spec Delta

## Purpose

为原生桌面验证码及其他分格文字录入提供统一的 Unicode 候选、组内焦点与完成通知合同，使部分输入、格式化、IME、动态容量及主题呈现共享既有输入基础，并保持组件与会话生命周期一致。

## ADDED Requirements

### Requirement: Typed OTP 与 reactive 配置

系统 SHALL 提供 OTPProps/OTP/OTPRef 和 indexed typed separator，默认 length=6、mask 关闭，支持三个 ControlSize、四个 InputVariant、status、disabled/readOnly、direction 和原生输入提示。value 与 defaultValue MUST 互斥，length MUST 在 1–1024 范围；非法 reactive 配置 SHALL 保留已接受状态，挂载非法配置 SHALL 不泄漏资源。

#### Scenario: 配置拒绝与恢复
- **WHEN** 存活组件收到 length=0 或非法 mask，之后收到合法配置
- **THEN** 非法配置被拒绝且旧值/身份保留，合法更新继续生效

### Requirement: Unicode 分格与容量

系统 SHALL 将 CR/LF 去除，按 grapheme 将初值、外部值和用户候选分格，不拆开组合字或 emoji 序列。用户输入 SHALL 按当前容量截断；单 grapheme 替换 SHALL 保留其他格，多 grapheme 输入/粘贴 SHALL 从当前格覆盖并替换尾部。formatter SHALL 接收代表空洞的空格候选，结果验证后再发布；外部 authoritative 值 SHALL 绕过用户 formatter。

#### Scenario: 整段粘贴
- **WHEN** 在第 2 格粘贴包含组合字、中文和 emoji 的文本
- **THEN** 各 grapheme 占一个格子、保留前缀并截断超容量尾部，焦点前进到候选末端

#### Scenario: 异常 formatter
- **WHEN** formatter 抛出、候选 UTF-8/grapheme 验证失败或同步卸载/改写组值
- **THEN** 不发布旧候选，不产生部分组更新或残留 owner

### Requirement: 部分与完成通知

系统 SHALL 对成功用户编辑通知固定 length 的 String vector onInput，仅当所有格均非空且候选与编辑前不同才通知完整 String onChange。当前格字符不变而后续格改变的粘贴 SHALL 通知正确候选。初值、外部回写、length/Theme/mask 更新 SHALL 不触发用户编辑通知。前一回调卸载或以其他值替换事务 SHALL 取消后续旧完成通知和焦点移动；相同 controlled echo SHALL 保留事务。

#### Scenario: 由部分到完整
- **WHEN** 最后一格填入字符且其他格均有值
- **THEN** 先通知部分数组，再通知一次完整候选；重复同值不再次通知完成

### Requirement: 组内焦点与原生输入

系统 SHALL 支持 OTPRef bound/focus/blur、mount-only autoFocus、焦点及点击全选、前方空洞重定向、输入自动前进、左右方向导航和空格 Backspace 返回，onFocus/onBlur SHALL 携带格子索引。IME 活跃时 SHALL 保留当前格 owner/导航权，候选不分发或通知完成，提交后才前进；stale stamp SHALL 被拒绝。OTP SHALL 阻止单格 primary undo/redo，Tab SHALL 维持正常资格遍历。disabled SHALL 拒绝编辑并清理焦点/capture，明文 readOnly SHALL 允许选择/复制。

#### Scenario: IME 与空格导航
- **WHEN** 当前格存在未提交 composition，之后提交一个中文 grapheme 并在空格按 Backspace
- **THEN** composition 期间不前进，提交才通知并移动，空格 Backspace 转到前格全选

### Requirement: Mask 与 typed separator

系统 SHALL 支持关闭、默认 bullet 与自定义单 grapheme mask；mask SHALL 仅改变显示和原生敏感输入提示，不改变 committed 值/回调，并按 Password 合同阻止明文 copy/cut。被动 indexed separator SHALL 挂载在相邻格子之间并继承主题，不引入独立编辑器或焦点入口。

#### Scenario: 保留原值
- **WHEN** 已有六格值切换到自定义 mask，再恢复明文
- **THEN** scene 不暴露明文 mask 内容、各格原值/回调/身份保持，恢复后显示原值

### Requirement: 动态长度与 retained 呈现

系统 SHALL 在 length 改变时保留存活前缀格子的 editor/scene/ref 与 separator 身份，仅增删尾部；删除拥有焦点/IME 的格子 SHALL 取消旧会话并按剩余资格转移。字体/Theme/variant/status/mask/value 普通更新 SHALL 不执行无关 Content。视觉 SHALL 使用 logical retained scene 与共同 renderer 资源，清理 SHALL 无残留 editor/scene/interaction/animation/ref。

#### Scenario: 缩短活跃组
- **WHEN** 第六格正在 composition，length 从 6 改为 4
- **THEN** 前四格身份不变，尾格资源/旧 stamp 退休，剩余组可继续输入

### Requirement: 分平台证据

平台通用逻辑 SHALL 用实际受支持 preset 完成一次；Windows/Linux 的 window、GPU、shader、system font、input、DPI/resize SHALL 各自以实际机器证据验收。缺少 Linux 运行 SHALL 保留独立未完成项，不以 Windows 或 planning 代替。

#### Scenario: Windows 验收完成
- **WHEN** Windows 两配置和真实 GPU 窗口均通过而 Linux 尚未运行
- **THEN** 保留 Windows 完成与 Linux 未完成的独立状态和可复核证据
