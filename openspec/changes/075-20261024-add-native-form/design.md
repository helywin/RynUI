# Design

## Context

参见 proposal.md。发现结果：Input/Select 使用 InputRef；Checkbox 有独立 CheckboxRef；InputNumber 与 Input 共用 editor 和 ref。控件的 value/checked 与 onChange 必须显式连接，且 InputNumber 保留未完成数字草稿。WindowComponentServices 共同管理组件生命周期、branch active、Modal 边界和 Spin input block；新 host 应复用这些能力，不注册窗口 Enter filter。Form support overlay 当前为 planned；本次按委派范围保留它供父线程整合。

视觉基线锁定 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd`，来源 `components/form/style/index.ts` 及已有 sources.lock/catalog。Context7 查询提供行为参考，版本锁定源优先。

## Goals / Non-Goals

**Goals:** 完整纵向原生表单、可编译显式绑定示例、增量状态与真实控件端到端 headless 验证。

**Non-Goals:** proposal 列出的扩展不进入首版；不修改现有控件默认键盘行为，不提供隐式全树采值、自动禁用任意 slot 或完整 Web name/required 语义。

## Decisions

1. `FormField<T>` 支持闭合类型集合 String、optional<double>、bool、SelectSelection。值由 field handle 保留，初值在创建时固定；可选择 Prop<T> 与 onChange 的外部受控模式。`value()`、`onChange()`、`status()`、`disabled()`、`readOnly()`/`uneditable()` 是显式适配入口。拒绝不同 field 重复 name、同一 field 同时重复注册和嵌套 Form。比任意 JSON/store 反射更贴合既有 typed API。
2. `FormItemProps::field(name, field)` 在最近的 Form scope 中注册；注册序列决定结果和焦点顺序。卸载立即移除结果和错误，handle 保留值；再次挂载创建新 registration ticket。隐藏/被阻止的已注册字段保留值及校验，但不获得焦点；整个表单被隐藏/Modal 排除/Spin 阻止时 ref 验证与提交拒绝执行。disabled 阻止提交及编辑回调，readOnly 阻止编辑但允许提交；程序 set/reset 不受编辑 gate 限制。
3. required 对 String 检查空串，对 optional 检查空值，对 bool 要求 true；数字必须有限。custom validator 返回空 String 表示通过，异常向调用者传播且不提交部分错误。校验只在 validate/submit 显式触发；值改变清除该字段错误，不重跑 validator。校验 snapshot + revision 防止回调改值/卸载后提交旧结果；旧事务返回 out_of_date，仍存活时进入失败回调。递归 validate/submit/reset 拒绝执行。
4. FormRef 为带弱生命周期的命令对象，提供 bound/validate/submit/reset；提交回调取得拥有值的结果快照。FormItem 的 focus 回调显式调用真实 InputRef/CheckboxRef；submit 依次尝试无效且可交互字段，不能越过 branch/input/Modal 边界。保持控件自身焦点资格检查，不增加私有路由。
5. Form/FormItem 使用 retained wrapper 和原生 Text；标签、必填标记、help/error 通过 Signal/Prop 更新，不重执行 content slots。空标签/反馈行退出布局流，真实 node 的 layout_size/out_of_flow、branch visibility 与 ancestor clip 一致。只实现纵向布局；外部尺寸由 LayoutStyle 控制。
6. 新增 typed Form Theme colors/metrics、override/algorithm、subscription/serialization，新增 snapshot 解析块使用引用捕获 IIFE，保留既有 24 顺序块及 Theme bridge/commit_snapshot 栈修复。金样除 identity 和新 Form keys 外所有值语义相等。

7. 真实 InputNumber 的相同数值回写会保留未完成草稿，因此 FormItem 提供 `control(InputRef/CheckboxRef)` 显式适配；InputRef 适配额外使用带现有 ref 生命周期的内部 reset hook，reset 取消 preedit 并将数字草稿格式化为当前 authority。普通 validate/render 不调用此 hook。未提供 control 的自定义 slot 只重置 typed 值；不虚构对任意 control 的草稿控制。

适配在 content 挂载结束后捕获 ref 的具体代际，避免重绑后的旧 FormItem 操作新控件；失败聚焦通过内部 FormFocusContext 把当前窗口/viewport/document clip 传给 ref 所属 host，检查该代际真实控件 root node 的 bounds、ancestor clip 与 input boundary，不以全宽 control region 代理窄控件。普通 InputRef/CheckboxRef 的逻辑 focus 合同保持不变。自定义 focus 回调仍须负责真实目标资格，Form 只检查 control region。编辑回调只依赖逻辑输入边界。String reset 同样重读既有受控 Prop authority，以正确处理应用拒绝回写的文本草稿。Form token 的语义字体/颜色只传给标签和反馈，control slot 保持既有控件的 Theme；最终派生 font_size 统一要求有限且 >= 0.5 logical dp，覆盖 override、继承及 component algorithm，防止 Text 在 DPI 换算前取整为零。

组件 algorithm 关闭时局部 seed 不参与派生或继承判定；父级 Form token 覆盖继续继承，再叠加子级显式 token override。响应式修改未启用 seed 不改变 snapshot 或通知；重新启用算法才使用 seed，关闭后恢复父级覆盖，inherit=false 仍按独立主题解析。

## Risks / Trade-offs

- 用户回调可重入及同步销毁 → 复制 callback/value snapshot，shared model + weak host lifetime、registration generation 和 revision 校验；销毁后 held refs/旧 change callbacks 惰性失效。
- 受控 reset 依赖应用回写 → 仅发初值 callback，不伪造外部 authority；文档和测试明确拒绝回写的行为。
- required 与自定义规则跨字段读依赖 → 校验事务检测改值，不提供自动 dependencies 或异步调度。
- 首版布局覆盖有限 → 明确纵向与无自动滚动边界；仍验证实际节点/glyph clip、窄宽及 token 更新。

## Migration Plan

恢复执行后用户指定：host 阶段验证并提交干净后，显式本地 merge `4efa310a256019192e01f0738ee226d16ad44396`，不 rebase；保留父基线新增的 Empty/Result、26 个既有 Theme IIFE、Theme binding 栈修复、各 host 及 wheel/Table 修复，再做定向组合验证。审核由父线程安排，不在此工作树创建 children。

纯新增 API，无依赖/ABI 后端修改。按规划、模型、集成验收分阶段本地英文 Conventional Commits；用户明确禁止 push/远端分支/PR/archive，父线程负责合并。独立构建树使用官方 windows-msvc-headless、MSVC、Ninja Multi-Config，Debug/Release 定向 CTest，最多 4 jobs；只可复用依赖源缓存，不复用其他构建树二进制。无 native GPU/Linux/系统 IME 验收声明。
