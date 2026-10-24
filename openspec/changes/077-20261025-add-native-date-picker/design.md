# Design

## Context

仓库没有公开日期类型或 DatePicker。Select 已展示保留 Input、窗口层和受控开合；Modal/Dropdown/Table 最近修复了实际 top hit 归属及内容优先滚轮阶段。Input 已具备受控编辑、提交、clear、ref、IME 与状态/变体；本 change 复用这些合同。已核对本地锁定 6.6.5 date-picker/index.en-US.md、style/index.ts，遵循其单日期和主要 token 语义，不引入 React/Day.js。

## Goals / Non-Goals

**Goals:** 无时区公历日期、ISO 编辑和真实 42 格月历；可用的范围限制、键盘、开合与嵌套输入边界；有限保留场景和统一 Theme。

**Non-Goals:** RangePicker、多选、时间、周/月/季/年选择模式、自由格式解析、locale/RTL/时区转换服务、复杂自定义单元格/面板、预设、动画、Web aliases 或辅助技术桥接；本阶段不修改全局 Gallery/support overlay，不要求重复平台通用全套测试。

## Decisions

1. 公开有效公历年月日与 optional 选择，模型负责闰年、月份天数、比较、月份导航、六周网格和 ISO 解析/格式。与 epoch/time zone 分离；固定有效范围 1..9999，边界月份不生成非法日期。面板可显式给定初始日期；无值无显式面板日期时按运行时 UTC 当前月初始化，文档明确来源，不伪称本地时区“今天”服务。测试使用显式面板日期，避免依赖系统时钟。
2. Input 负责实际文本编辑与场景，DatePicker 维护已选值、草稿提交和面板浏览三类状态。Enter/blur 提交遵循未完成 IME 门禁，无效 blur 草稿恢复当前值；inputReadOnly 只禁止手工编辑，不阻断允许的月历选择。value/open 受控回调去重与实际失活 suppression 分离，避免拒绝回写时遮挡模态输入。
3. 月历为固定保留网格，文本/eligibility 随月变化，不重复挂载任意 slot；typed footer 只挂载一次。使用实际交互/焦点记录，月历键盘为有限的一组入口，禁用全范围时不做无界扫描。选择、清空、跨月及回调重入需在 generation/lifetime 检查后继续访问状态。
4. 复用现有窗口层/布局定位、Input/ref 与 Theme 服务，单独 DatePicker host/model。保持 Modal → 内容容器 → popup 定位和内容滚轮优先于边界兜底的合同；更高 popup 归属由实际命中链决定，不用矩形范围强吞其他弹层。短/零可用高度设置实际 clip 和不可命中状态，保留子控件本身身份。
5. 新增 typed Component Token 的默认、派生/继承、algorithm、hash/equality、diagnostic 和 dirty 链路；复杂 Theme 解析继续使用 reference IIFE 控制栈使用。复用 Input 视觉入口，LayoutStyle 仅控制外部布局。五份 golden 由新可执行文件生成，按路径证明旧字段除 identity 外不变。
6. 平台通用验证使用 Windows MSVC、windows-msvc-headless preset 与 Ninja Multi-Config 的 Debug/Release。模型与实际节点/glyph/interaction 测试覆盖首版；平台窗口/GPU/DPI/系统 IME 与 Linux 证据独立，后续 Gallery 才提供用户入口和原生 smoke。本 change 不把 headless 结果描述成原生平台验收。
7. FormItem 通过 `control(DatePickerRef)` 显式连接 String 适配时，程序 reset 必须在聚焦且 field 值没有变化时同样清除未提交草稿。Input host 提供仅内部的 `configure_form_reset(ComponentId, std::function<void()>)`：先取消 preedit，再复制并调用复合 owner hook，随后重取相同 component/editor 代际及 active scope，继续既有数字/字符串 authority reconcile。DatePicker hook 重读受控 Prop 或保留非受控已接受值，清除 dirty draft 并格式化 display，不发 onChange/onOpenChange，不递归调用 Input reset。普通同值 Prop、校验/help/Theme 更新不调用此 hook；Form 现有 ref registration 代际和 component/editor 代际共同防止旧适配操作重绑控件，回调造成卸载后立即停止。

## Risks / Trade-offs

- [日期边界、闰年和解析错误] → 单独模型边界与跨月/跨年/世纪回归；禁止用整数天溢出或字符串猜测接受非法值。
- [受控拒绝和隐藏祖先复活弹层] → 共享边界合同并复现拒绝关闭、重复隐藏/恢复、未聚焦 popup 与回调销毁场景。
- [大 Theme 或局部 footer 导致栈/身份回归] → 保留顺序解析、实际嵌套 Theme 与 retained Input/footer 测试，旧 golden 语义比较。
- [首版未覆盖完整 Ant Design] → API 文档列出支持范围，不用 planned/support 状态代替代码；后续 TimePicker/RangePicker 独立增量。
