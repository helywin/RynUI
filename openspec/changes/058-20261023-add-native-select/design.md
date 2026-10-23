# Design

## Context

动机与首版范围见 proposal。Input 已有真实 Unicode/IME/clipboard/ref、四变体与清空；WindowComponentServices 已支持 window layers、participant pointer/keyboard/定位与 retained surface。尚无 Select，也没有共用的 participant scroll 入口；已有滚动事件从 Input host 进入。主规格目录为空。

## Goals / Non-Goals

**Goals:** 先让业务表单可用；值和标签分离，复用输入服务与窗口场景；不以普通更新重建组件或改变 renderer ABI。

**Non-Goals:** multiple/tags、分组选项、自定义 option render、虚拟化、内置异步请求和 Web API。它们保留在后续清单，不宣称首版完全覆盖 Ant Select。常用组件广度优先于 Theme 全局配置与既有组件长尾 API。

## Decisions

1. `SelectValue` 保存 String/int64，`SelectOption` 保存 value/label/disabled。`SelectProps` 用 `Prop<T>`、optional value、defaultValue、options、showSearch、allowClear、disabled/loading、ControlSize/InputVariant/InputStatus、open/defaultOpen 及回调；value/defaultValue 和 open/defaultOpen 冲突均拒绝。未知选中值暂存并显示序列化值，适应外部动态 options。
2. 预建 Input 和弹层子树。Input 负责选择器文本、真实搜索与 IME；非搜索时 readOnly。输入文本和 query/selected 分离，搜索不会隐式提交 value；通过装饰性 prefix/suffix typed slots 支持原生组合，默认离线 Down/Loading 图标。为维持单一 Tab 入口，首版拒绝 slot 中额外的 focusable 控件；交互式附件后续扩展。SelectRef 复用 InputRef 的 focus/blur/搜索选区。
3. Select 自有窗口层菜单与 retained surface，行 interaction 不增加 Tab 入口，焦点指向 selector Input，避免选项点击先触发 blur。窗口层定位基于 selector bounds+translation，选择可用的上/下空间并约束 viewport；菜单内部独立 clip 与滚动。只在 options 显式变化时更新候选子树，query/hover/活动项/选择只修改对应状态与可见性。
4. 新增 participant scroll 钩子及窗口服务分发，从既有 Input scroll 入口先询问弹层，未消费继续原来的 TextArea 路径。键盘 participant 只处理当前 selector；组合期间不截获 IME 键。其他组件或窗口不受影响，Escape 沿用057的文本编辑优先级。
5. Select Component Token 定义菜单颜色/行高/字体/间距/圆角/shadow/order，以及 selectorBg、clearBg、hoverBorderColor、activeBorderColor、activeOutlineColor。selector 复用 Input Component Token 的尺寸、变体与 status 配方，Select 的对应覆盖在内部 Theme 隔离；不开放 Props 视觉样式入口。按 colors/metrics/typography/effects/order 分组捕获 dirty domain，同步序列化/hash/golden。
6. common 在 Windows `windows-msvc-headless` Debug/Release 验收一次，含模型、真实组件、输入/IME、布局/局部性/生命周期；Gallery 合同在现有 Recording fixture 验证。Windows 使用 `windows-msvc` MSVC x64/Ninja Multi-Config/D3D12/DXIL 真实窗口与scale矩阵，Linux 单列原生验收，不用 WSLg 或 Windows 替代。

固定参考：[Select Token 6.6.5](https://github.com/ant-design/ant-design/blob/6.6.5/components/select/style/token.ts) 与 Context7 `/ant-design/ant-design` Select 文档。首版 optionHeight=controlHeight、padding 由字体行高与 controlPaddingHorizontal 推导、zIndexPopup=zIndexPopupBase+50；不引入 React/rc-select。

## Risks / Trade-offs

- 弹层点击与 Input blur 重入 → 保持单一焦点，回调后重新校验 generation。
- 动态选项或 controlled 拒绝导致 label/value 漂移 → 明确 value 为唯一选择来源，query 与活动项独立。
- 第一版非虚拟列表 → 如实记录性能边界，固定行高和局部失效；不承诺百万选项或未测容量。
- Theme 接线影响既有组件 → token分组精确、旧值保持、完整headless和Gallery合同验证。

## Migration Plan

规划、Token/API/模型、完整交互、Gallery/common与原生证据分阶段提交；每次提交立即安全 fetch/rebase、push、核验远端 SHA。不修改历史平台证据，不自动 archive。
