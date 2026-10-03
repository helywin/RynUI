# Design

## Context

动机见 proposal.md。Input 已提供事务化 Unicode 编辑、reactive 值、ref、共享 IME/clipboard、四变体和 retained scene；WindowComponentServices 支持附属宿主及动态 append_slot。OTP 需要组级候选与完成判断，而非六个彼此无关的业务输入框。

合同核对日期 2026-10-03，固定 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd`：[OTP](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/input/OTP/index.tsx)、[单格](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/input/OTP/OTPInput.tsx)、[样式](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/input/style/otp.ts)。借鉴原生适用的完成回调、导航及视觉合同，Unicode 分格使用既有 grapheme 边界。

## Goals / Non-Goals

**Goals:** 组内每个存活格子使用稳定 Input editor/IME owner；组级候选先格式化/验证再发布，各格值与焦点协调一致。只重建增删的末尾格子，不重新执行无关组件或 separator content。

**Non-Goals:** 无 Web DOM/HTML autoComplete/type、原生元素引用或 CSS 视觉入口；不在此 change 实现多字符 bidi shaping。OTP 阻止单格 primary undo/redo，避免历史破坏组一致性；一般 Input 历史不变。

## Decisions

1. 新增内部纯 OTP model，按 grapheme 拆分规范化单行文本。活跃 length 默认 6，支持 1–1024，容量上限与现有 retained 组合组件一致；外部值与配置更新不通知用户编辑回调。未编辑的外部完整值可保留超出当前可见容量的部分，扩大长度重新显示；用户编辑按当前容量截断。
2. 格子使用现有 Input 编辑/IME，OTP host 维护组状态、格子 ref/组件身份及 reactive 投影。Input 内部单格配置负责居中、Theme padding/尺寸、自定义 mask 和导航 hooks，不向公开 Input 增加 OTP 特例。备选单 editor 绘制所有格子会重新实现会话/命中；完全独立 Inputs 缺少组事务，因此使用共享组模型加既有单格宿主。
3. 编辑 transform 在本地 editor 发布前准备完整组候选；粘贴多 grapheme 从当前格覆盖并替换尾部，单 grapheme 替换保留其他格。formatter 接收以空格代表空洞的完整候选，格式化后按 grapheme/容量验证。successful user edit hook 在合法编辑后发布组候选，即使当前格最终字符相同但后续格改变也正确通知。composition update、authoritative 回写不走此入口。
4. 候选、mask 与 formatter 结果的 UTF-8/单行/grapheme 验证先于发布。版本/生命周期校验拒绝 formatter 重入、authoritative 变化或卸载后的旧候选。onInput 拿到固定 length 的 String vector，之后仅对填满且与编辑前值不同的候选调用 onChange；若前一回调卸载或用其他值取代事务，则取消后续完成通知和自动焦点。相同 controlled echo 保留本次事务。
5. OTPProps 使用 Prop 的 value/length/size/variant/status/disabled/readOnly/direction/mask/purpose/capitalization/autocorrect；defaultValue 与 value 互斥。mask 默认关闭，开启默认 bullet，可用一个 grapheme 自定义。OTPRef 提供线程绑定的 bound/focus/blur；焦点按既有 defer_focus/lifetime 规则处理，onFocus/onBlur 携带格子索引。mask 按 Password 合同禁止明文 copy/cut，不限制明确的值回调。
6. 首次焦点/点击格子选中全部字符；进入较后格但存在前方空洞时转到第一空格。输入后前进至候选末端；左右按组方向导航，空格 Backspace 返回上一格。IME 活跃时仍拥有导航/Enter，未提交文本不前进。Tab 沿正常资格遍历；autoFocus 只首次挂载的第一格。disabled 取消焦点/输入/capture，readOnly 可选择复制。
7. 组根使用原生水平布局和 Theme paddingXS gap；三个尺寸的单格 inline padding 按 Input tokens 推导，文本居中。四变体/status/禁用/主题继续由 Input 绘制，mask 不影响 committed 值或回调。可选 indexed typed separator 在格子之间挂载，允许被动内容，不增加焦点入口。动态 length 保留前缀格子与 separator，删除尾部清理 editor/session/scene/ref；新增部分单独 append。

## Risks / Trade-offs

- [本格字符相同而粘贴改变后续格] → successful edit hook 与 pending candidate 测试，不只依赖 local value_changed。
- [formatter/callback 重入或卸载] → 候选准备、版本与 component generation 校验、回调副本，覆盖异常/回写/销毁。
- [动态 length 退休当前 IME owner] → 先取消资格/会话，删除尾部再按剩余资格转移焦点；prefix 身份不变。
- [mask 长度不同于原值] → 复用逻辑与显示 byte 映射，单 grapheme mask 验证，原值不可泄漏到 glyph scene。
- [多 editor 成本] → 明确容量与资源上限；共同测试覆盖 idle/无关内容与复用，不把单行 benchmark 当作 OTP 性能结论。
- [动态格子 source 先于存活 observer 析构] → ReactiveSource 析构清理 observer 的借用依赖指针；覆盖 source 销毁后重算、排队通知、observer 内销毁以及无分配清理，避免卸载时悬空访问。

## Migration Plan

按 tasks 顺序小阶段提交。共同逻辑实际用 Windows `windows-msvc-headless` Debug/Release；Gallery Recording frame 合同可在 `windows-msvc` 完成一次。正式 Windows 构建为 MSVC/Ninja Multi-Config；window/system font/SDL input/DPI 与 D3D12/DXIL 按真实 Windows 运行。Linux `linux-native` 在真实 Linux 独立验收，本机不勾选 Linux 项。新 API 为加法，不增加依赖或改 GPU ABI。
