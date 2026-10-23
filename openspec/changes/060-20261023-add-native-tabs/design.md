# Design

## Context

见proposal动机。现有WindowComponentServices提供participant、retained surface、焦点/命中、scroll入口与分支可见性；Select/Card已验证隐藏面板嵌套输入/窗口层清理。文本服务有单行省略和可patch的逻辑平移。当前没有Tabs公开API，主规格目录为空。

已读取Context7 `/ant-design/ant-design` Tabs API并核对本地锁定6.6.5的`components/tabs/index.en-US.md`、`style/index.ts`。上游已有tabPlacement，line为默认；horizontalItemGutter固定32、三尺寸vertical padding为paddingXS/SM/default，card高度32/40/48及gutter2；选中颜色和inkBar来自primary。只移植原生合同，不引入rc-tabs或Web别名。

## Goals / Non-Goals

**Goals:** 常见静态业务页面切换、明确受控语义和可访问溢出；复用现有窗口/布局/输入/场景所有权，不增加renderer ABI。

**Non-Goals:** proposal所列扩展。首版所有面板预建，相当于保留内容模式；集合结构固定，仅label/disabled响应式，避免为动态增删设计未被需求使用的内容diff系统。

## Decisions

1. `TabItem(String key, String label, TabContent)`配合`.label(Prop<String>)`、`.disabled(Prop<bool>)`；key不可变。`Tabs(TabsProps, std::vector<TabItem>)`提供activeKey/defaultActiveKey、ControlSize、TabsType(Line/Card)、TabsPlacement(Top/Bottom)、centered与回调。key仅String，空/重复key预先拒绝；label为空时显示key。不暴露通用样式Props。
2. 纯选择模型独立验证：无显式key选首个enabled，显式key原样保存，未知key无面板；disabled只禁止用户激活，不偷偷改变既有值。controlled请求先发onChange，父级拒绝则面板不变；重复点击只发onTabClick。activeKey/defaultActiveKey同时存在拒绝。键盘焦点与选中key可分离，受控拒绝不夺走用户焦点。
3. WindowComponentServices拥有Tabs participant。一次挂载所有header和typed面板；根自定义Constraints布局只测量当前面板，inactive分支不参与场景/命中。切换通过共同焦点协调清理旧Input和嵌套Select，不销毁面板，不自动把焦点送进新面板。所有回调后重查owner/generation。
4. 标签本身是retained surface/interaction和语义Text，普通标签单行省略；label/disabled直接更新对应行。焦点落在enabled header时以它为组Tab入口，否则用enabled active或首个enabled。Left/Right循环、Home/End定位并请求激活；Enter/Space请求当前项，面板Input中的方向键不被截获。
5. header视口独立clip，前后箭头预建且非Tab入口，滚轮只消费标签区域；溢出时显示箭头，按可用宽度分页滚动。活动/键盘焦点项或resize需要时显露；手动滚动不每帧被当前选中项拉回。标签子树使用逻辑translation，保持文字owner并避免滚动重塑字形。centered仅在没有溢出时生效，Top/Bottom共享一套布局合同。
6. Tabs Token分colors/metrics/typography/effects，覆盖普通/选中/hover/active/disabled、指示条/rail/card背景/边框/focus颜色，line/card三尺寸、gutter/body gap/箭头/焦点留白、字体与focus效果；接入既有继承、独立seed/algorithm、override、hash/序列化和golden。颜色不测量，效果只修改几何/材质，字体/尺寸局部测量。背景/指示条/边框沿用共同rounded effects和祖先clip。
7. common在Windows `windows-msvc-headless` Debug/Release完成一次；Gallery沿用Recording合同。Windows `windows-msvc` MSVC x64/Ninja Multi-Config/D3D12/DXIL真实窗口验证主题/scale矩阵；Linux native/Vulkan单列实际证据，不能用Windows或WSLg代替。

## Risks / Trade-offs

- 固定集合且所有内容预建占用内存 → 明确首版边界，不称为lazy或虚拟化；避免为尚无使用方的动态内容设计diff。
- 受控拒绝引起焦点/面板不一致 → 显式分离，Tab入口跟随当前header焦点，离组后以选中项恢复。
- 标签clip与滚动命中错位 → 相同logical translation与NodeStore clip用于绘制/命中，测试滚轮、箭头、键盘和resize。
- 隐藏内容blur/用户回调卸载owner → generation检查与真实嵌套Input/Select及回调销毁测试。
- 新Token导致全局identity改变 → 更新golden并按值对照前序提交，禁止其他组件配方漂移。

## Migration Plan

规划、API/模型/Token、retained交互、Gallery和Windows/Linux证据分阶段提交；每次立即安全fetch/rebase/push并核验远端SHA。不自动archive。Linux缺少机器时保留未勾选，继续其他常用组件。
