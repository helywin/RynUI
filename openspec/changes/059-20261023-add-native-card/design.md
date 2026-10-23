# Design

## Context

见 proposal 的动机与首版范围。现有公共 API 没有 Card。WindowComponentServices 已提供组件 participant、retained surface、子树 active、语义文字样式和焦点/命中服务；Divider 使用自定义测量与几何同步，适合作为容器布局参考。Gallery 已有 Card 参考条目但仅为 planned。主规格目录为空。

已读取 Context7 `/ant-design/ant-design` Card API，并核对本地锁定 `out/upstream-ant-design-6.6.5/components/card/style/index.ts`：默认 header 高度为大字号行高+2×padding，小号为默认字号行高+2×paddingXS；small body/header padding 固定12，默认为 paddingLG，header 默认透明，actions 为容器背景。

## Goals / Non-Goals

**Goals:** 以一个 retained 容器提供可组合区域，保证嵌套 Input/Button/Select 的窗口服务和 scope 不变。不引入其他组件私有宿主、依赖或 renderer 上传。

**Non-Goals:** proposal 所列长尾能力；本阶段不会为了 Card 提前实现 Tabs、Grid、Meta 或通用 Skeleton 系统。静态三行占位即可表达 loading。

## Decisions

1. `CardProps` 提供 title、CardSize(Default/Small)、CardVariant(Outlined/Borderless)、hoverable、loading、layout；使用 `CardContent` 与 `CardSlots` 中的可选 CardTitle/CardExtra/CardCover/CardActions。title slot 存在时优先于 title 文本，避免额外动态重建入口。任意原生控件可放进 slot；Card 只提供语义文字默认值，不改写其视觉 Props。
2. WindowComponentServices 拥有 Card participant；root 自定义 Constraints 布局。预建 header/title/extra、cover、body、占位和 actions 分支。header title 占剩余宽度，extra 先受可用宽度约束，header 高度取 Token 最小值和子内容实际高度的最大值；无标题/extra 时去掉 header。内间距来自 Card Token，body/cover/actions 自然高度；最小尺寸受 constraints 约束。
3. loading 只切换 body 与静态占位分支，保留 cover/header/actions 与用户 body owner。分支隐藏后使用共同焦点协调完成 blur/输入会话清理；恢复时不自动夺焦点。用真实编辑值与 identity 验证，避免单纯核对内部 bool。
4. 使用共同 retained surface 创建背景/边框、header/actions 分隔线、占位条与多层 shadow；无通用 Modifier、无私有 GPU 路径。Card 背景 interaction 非 focusable，fragment 在 children 前面；pointer participant 根据当前命中组件祖先判断 hover，所以内部 Button 仍能命中和激活。视觉有限更新不创建动画轮询。
5. Card Token 分 colors/metrics/typography/effects，包含 header/extra/body/actions/placeholder 颜色、尺寸与圆角、两尺寸字体/字重、普通/hover shadow。复用既有 snapshot、主题继承与 component seed/algorithm 管线，添加4个精确 dirty identity；颜色不 remeasure，effects 不改变布局。
6. common 使用 Windows `windows-msvc-headless` Debug/Release 验证一次，Gallery 使用现有 Recording 合同；原生采用 `windows-msvc` MSVC x64、Ninja Multi-Config、D3D12/DXIL 窗口，Linux 按 native preset/Vulkan 独立验收，缺少实际机器不影响后续 Tabs 工作。

## Risks / Trade-offs

- header 附件或子内容超过 constraints → 测量阶段先约束 extra，再给 title 剩余宽度，有限宽度回归覆盖空/长标题。
- loading 中仍保留正文订阅与数据 → 这是恢复业务状态的取舍；分支不可见/不可交互，不宣称虚拟化或节省未挂载内存。
- hover 背景命中遮挡子控件 → fragment 顺序与祖先识别验证；不新增点击卡片动作，业务动作放真实 Button/Link。
- 新 Token identity 改变 snapshot hash → 更新 golden；按值比较既有组件，禁止旧配方漂移。

## Migration Plan

规划、API/Token、容器与交互、Gallery、Windows/Linux 独立证据按阶段提交，每次立即安全 fetch/rebase/push 并核验 SHA。无破坏性迁移；新 API 加入 umbrella header。未完成的平台项保持未勾选，不自动 archive。
