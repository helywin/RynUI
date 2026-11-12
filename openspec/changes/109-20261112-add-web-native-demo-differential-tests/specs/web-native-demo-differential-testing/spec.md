# Spec Delta

## Purpose

为 RynUI 当前 C++ Gallery 与固定版本官方 Web demo 建立可重复的自动差分测试，使覆盖、动作执行、画面与行为判定、失败定位及修复回归均可由机器完成，并明确参考来源、平台证据和无法自动等价的边界。

## ADDED Requirements

### Requirement: 参考来源不可漂移
测试系统 MUST 锁定 Ant Design 6.6.5 和 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd`，校验 demo、组件依赖源码、Token、资源、测试依赖与浏览器的版本及内容指纹。参考缓存 SHALL 可由锁定输入重建；实时官网或 native 输出不得作为隐式参考替代物。

#### Scenario: 缓存来源变化
- **WHEN** 缓存中的 demo、依赖、字体、浏览器或参考算法指纹与请求不符
- **THEN** runner 拒绝该缓存并给出 reference drift/environment 错误；只有从正确锁定输入重新构建后才允许比较，不以旧图继续通过

### Requirement: 有序完整覆盖账本
系统 SHALL 以实际 catalog、canonical 页、家族、sample/source 和 target writer 的完整有序身份集合建立多对多映射及覆盖账本，不以历史数量作上限或仅检查 count。每个 case MUST 有稳定 identity、来源、变体、动作/检查点及独立预期；每个已有 native 范围 MUST 被覆盖或报告具体缺口。

#### Scenario: 新增或替换样例
- **WHEN** 当前 writer 新增样例，或删除一个旧 target 后添加另一个使总量不变
- **THEN** inventory 校验准确列出新增、删除、顺序及映射差量，缺失映射使 coverage gate 失败；不能更新一个数量字面值或放宽到任意数量就通过

#### Scenario: Input 与关联样例
- **WHEN** 枚举 Input、Search、Password、TextArea、OTP 和跨组件组合
- **THEN** 只计算一个 canonical Input 目录项，分别记录家族和 sample；官方 demo 与多个 native 变体的对应关系不相互覆盖或重复充当已执行项

### Requirement: 边界说明不能消除既有原生覆盖
系统 SHALL 区分 paired、native-only、web-only、unsupported 与待实现的映射，不把它们全部计为 paired pass。边界 MUST 指明 feature/原因/来源/适用维度，保留可执行的单侧合同；运行失败、缺 oracle 或尚未实现的 adapter 不得改标 unsupported 来通过。

#### Scenario: List 与 Util
- **WHEN** 检查 deprecated List 说明页及 web-only Util 类型工具页
- **THEN** 自动检查说明页、来源和无伪造交互样例，记录没有视觉组件对应关系；这不减少其他组件已有 native sample、逻辑或视觉检查的分母

### Requirement: 对照输入等价且改写可追踪
每次 paired 执行 MUST 使用同一数据、公开 Props/slot 内容、布局约束、主题、语言、format locale、方向、字体策略、DPR、初始状态和语义动作序列。官方原样 demo 与按原生组合进行参数化的官方组件场景 SHALL 分开标识，所有参数化差异必须可审计，不得修改参考样式以迎合 native。

#### Scenario: 两侧初始化不一致
- **WHEN** 两侧实际宽度、文本、选项顺序、主题或初始值与 case 声明不同
- **THEN** 在比较前失败并指出具体字段，不把条件不一致的画面归因为 native 视觉缺陷，也不靠图像拉伸对齐

### Requirement: 单命令自动完成与失败关闭
runner SHALL 从 case 选择自动完成参考准备、双侧执行、采集、判定和机器报告。仅当所有选中检查点、动作回执、oracle 和必要产物完整且通过时才返回零；空选择、超时、崩溃、缺页/缺图/缺 oracle、未执行和中断 MUST 返回非零及具体状态。

#### Scenario: 进程零退出但未执行
- **WHEN** adapter 返回零却缺少一个动作回执、检查点或该 run 的新帧
- **THEN** 总结果为 incomplete/error，报告缺失身份；不能因为进程成功或旧目录中有截图而判 pass

### Requirement: 隔离运行不干扰桌面
Web SHALL 使用独立 headless browser/context；Linux native SHALL 使用私有 Wayland/DBus 与隐藏窗口，Windows SHALL 使用隐藏原生窗口。执行 MUST 有进程树超时清理及无桌面回退策略，不使用物理输入、用户浏览器会话或全局光标。

#### Scenario: 后台环境不可用
- **WHEN** 私有 compositor、必需 GPU backend 或浏览器启动失败
- **THEN** 返回 environment 错误且非零退出，清理本次拥有的进程与临时资源，不打开用户桌面、不标为测试通过

### Requirement: 原生证据来自生产调用链
native adapter SHALL 复用现有页面挂载、有限输入分发、按需帧泵、动画时钟和共同 renderer 的 GPU readback；不得另造简化组件、私有上传或第二渲染/输入框架。输入层级、提交 revision、GPU 完成/回读及是否实际呈现 MUST 分别记录。

#### Scenario: 隐藏 GPU 采集
- **WHEN** 正常组件经过有限语义输入后采集隐藏窗口画面
- **THEN** 报告能关联该动作、状态、scene revision 和已完成 GPU 回读；不将 readback 记为显示器呈现或 compositor callback 通过

### Requirement: 分层视觉判定必须保留缺陷敏感度
系统 MUST 分别判断布局/尺寸/间距/对齐/clip/overflow、文字/图标、Token/颜色/边框/圆角/阴影与像素合成。所有必需层均通过才可视觉通过；几何/语义相等不得替代实际可见内容。容差 SHALL 按维度、材料、字体与平台校准，禁止全图宽松阈值、图像变形或消除全部文字/边缘的 mask。

#### Scenario: 小控件的大背景
- **WHEN** 一张大画面中仅一个小图标消失、一个输入框裁字或一条边框错位
- **THEN** 对应语义 ROI 的存在性、局部几何/墨迹/边缘检查失败，不得被全图平均相似度稀释为通过

#### Scenario: 出界绘制
- **WHEN** 控件声明 clip 正确但实际 GPU 像素泄漏到 clip 外
- **THEN** 外围检查带或上下文图像自动报 overflow/clip 失败，不能仅裁取 native 自报可见矩形而遗漏错误

### Requirement: 容差有来源且不能为失败自动放宽
系统 SHALL 使用版本化比较 profile、同源重复测量及已知正负控制自动校准噪声，保存阈值来源、硬上限和适用环境。校准 MUST 独立于待判 native 差异；合法重复噪声与最小故障不能分离时必须报 calibration error，不能提升阈值或生成 pass。

#### Scenario: 过度 mask 或阈值
- **WHEN** profile 屏蔽关键文字/图标/边框、覆盖面积超限，或放宽阈值导致规定故障注入未被检出
- **THEN** profile/meta-test 失败并阻止该 profile 输出通过结论

### Requirement: 文本字体与色彩域可核验
系统 MUST 记录两侧实际字体 face/hash、字号/字重/字形覆盖、文字内容与行布局，并声明 framebuffer 编码、alpha 和色彩转换。固定字体与系统字体证据 SHALL 分开；字体缺失、替代字符或色彩校准失败不能用栅格差异解释为通过。

#### Scenario: 栅格不同但内容一致
- **WHEN** 相同字体文件在两侧存在受限抗锯齿差异
- **THEN** 严格检查文字、行数、基线/边界、裁切和墨迹存在性，再按校准 profile 检查局部覆盖；不屏蔽整段文字、不宣称逐像素相等

### Requirement: 逻辑使用独立语义 oracle
系统 SHALL 将两侧观察值规范化为稳定语义 target/key/value 与有序事件 trace，再分别验证独立声明的合同及双侧关系。native 观察器不得生成 expected；归一化不得吞掉值、callback 次数/顺序、焦点变化或关键生命周期事件。

#### Scenario: 两侧都返回错误值
- **WHEN** Web adapter 与 native adapter 的结果恰好相同但违反声明的受控 accept/reject 合同
- **THEN** 独立 oracle 仍失败，不能仅用两份 trace 相等判通过

#### Scenario: Provider 字段并非所有组件都消费
- **WHEN** 改变 Provider size/disabled/direction 并验证 Progress 与真实消费这些字段的控件
- **THEN** 按各组件固定版本合同分别断言不消费字段保持不变、消费字段更新及显式覆盖优先；不能统一生成全体组件继承 size/disabled 的错误预期

### Requirement: 输入与状态变化可自动比较
case 矩阵 SHALL 覆盖适用的 controlled accept/reject、focus/Tab/Shift+Tab/Escape、popup/childModal、wheel 消费与边界、disabled/loading、隐藏恢复、编辑/选区，以及 locale/方向/Theme 变化。每步 MUST 有实际分发与观察回执，不以直接改最终值代替待测输入路径。

#### Scenario: 禁用后仍触发
- **WHEN** 对 disabled 控件发送已声明的点击或键盘动作
- **THEN** 动作回执显示输入确已发送，oracle 断言值、focus 规则和 callback 计数符合禁用合同；跳过动作不能满足此负向断言

### Requirement: 合成编辑与系统集成证据分离
自动编辑测试 SHALL 将 DOM/browser protocol、native core input dispatch、SDL event mapping 与 OS IME/AT 证据分层标注；自动 composition seam MUST 覆盖会话代际、选区、提交/取消及迟到事件，不把合成事件冒充实际系统候选窗或辅助技术行为。

#### Scenario: 旧组合会话迟到
- **WHEN** 在 native 编辑器有 preedit 时切页，再向旧会话发送 commit
- **THEN** 新页值、focus 与 callback 不受影响，旧会话已退役；报告仅声明自动合成路径通过

### Requirement: 确定时钟与生命周期 oracle
系统 SHALL 对适用动画/异步任务使用可控时间及显式完成顺序，检查起点、中间态、终点、反向、hidden/destroy 和 idle；动态 key/草稿、迟到完成与离页清理必须有独立 oracle。不能用固定 wall-clock sleep 或只检查终点替代中间态。

#### Scenario: 反向与离页
- **WHEN** popup 动画中途反向，随后离页并推进到旧任务的完成时间
- **THEN** 对应检查点几何/状态/事件满足合同，旧 owner 无活动输入/动画/提交，不污染新页；合法共享缓存不误报泄漏

### Requirement: 差异报告可复现并分类
每次执行 SHALL 输出机器可读 run/case 结果与本地索引，包含 source/配置/二进制/环境指纹、动作及首次失败检查点、实测与预期、原图/差图/几何/trace 路径和复现命令。分类 MUST 至少区分 visual、layout、typography、token、state、input、logic、lifecycle、environment 与 reference drift；推测原因不得冒称确定根因。

#### Scenario: 修复工作消费失败
- **WHEN** 一个 case 出现 callback 顺序差异和视觉变化
- **THEN** 报告保留两类事实、首个分歧及目标源码映射，Agent 可直接选择失败 case 重跑，无需重新人工采图或逐图裁决

### Requirement: 回归与基线迁移分离
普通修复 SHALL 自动重跑失败 case 与必要依赖影响集，使用同一参考版本及比较 profile。参考版本迁移 MUST 独立记录变更来源、重新校准及 meta tests；不得因 native 失败自动覆盖官方 baseline、更新 expected 或扩大排除范围。

#### Scenario: 更换 native 二进制
- **WHEN** 修复后运行报告中的 replay 命令
- **THEN** 生成新 run 并记录新二进制身份，保留原 case/动作/参考输入和旧失败证据；若请求参考已漂移，明确拒绝比较

### Requirement: 机制必须被真实故障注入证伪
系统 MUST 自动运行真实执行路径中的外观与逻辑故障注入，证明原命令在故障下非零、恢复后通过；还须拒绝空采集、错版本、漏 case、崩溃、不执行动作及过度容差。只修改最终报告或图片文件的测试不能替代真实组件路径的注入证据。

#### Scenario: 外观和逻辑红绿闭环
- **WHEN** 在 pilot 的真实组件路径分别注入 padding/颜色/圆角/裁剪/图标/字体故障及受控值/callback/focus/disabled 故障，再恢复
- **THEN** 每种规定故障均由相应机器判定检出，未注入执行及恢复后同命令绿，报告保存每次退出码与检出层；未检出则机制验收失败

### Requirement: 频率预算与平台验收独立
系统 SHALL 按 case 依赖选择定向回归，明确共享变更扩展规则、全量 CI 入口、超时/资源预算与取消结果。平台通用合同只验收一次；Linux/Vulkan 与实际 Windows/MSVC/D3D12 的 OS/GPU/font/input 集成结果 MUST 分开，不互相替代，也不自动创建定时任务。

#### Scenario: 共享渲染或字体变更
- **WHEN** 改动影响多个组件的 renderer、字体、Theme、输入或比较器
- **THEN** 自动选择相应跨组件哨兵及依赖集，无法确定影响时选择完整适用集合；预算超限以未完成失败报告，不裁掉剩余 case 或宣称全量通过

### Requirement: 原始产物与产品依赖隔离
源码下载、安装目录、截图、缓存、运行 JSON/日志与差图 SHALL 只写 ignored `out/`，版本化 schema、确定性输入、可复用测试代码及简短结论可入库。Web 运行依赖 MUST 与原生产品隔离，测试工具缺失不得使 HEADLESS Core 偷偷引入 Node、DOM、React 或平台 renderer。

#### Scenario: 无 Web 工具的 Core 构建
- **WHEN** 使用正式 HEADLESS preset 构建平台通用生产 Core
- **THEN** 不需要安装 Web 工具链且保持原 include/link 守卫；运行差分测试时显式准备锁定依赖，缺失则由差分 runner 报准备失败
