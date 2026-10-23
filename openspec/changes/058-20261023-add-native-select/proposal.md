# Proposal

## Why

用户优先需要能搭建业务页面的常用组件，当前缺少 Select 等表单与页面基础。先提供可以真实选择、搜索、清空并受控回写的原生单选 Select，再按常用组件清单推进其他控件，Theme 公共配置后置。

## What Changes

- 新增 typed `SelectProps`、稳定 value/option/ref 与 prefix/suffix slots，首版覆盖单选、动态选项、禁用、搜索、清空、loading/empty、受控/非受控值、打开状态与回调。
- 提供窗口层下拉列表、键盘导航/提交/关闭/Tab、长列表滚动和边缘翻转，保留输入、IME、焦点与组件 identity。
- 建立 Select Component Token、三尺寸/四变体/status、Theme 继承与 overrides，复用现有 Input 和 retained scene，不增加依赖。
- Gallery 与文档如实记录首版范围；multiple/tags、分组/自定义 option render、虚拟列表和异步数据源放在后续扩展，不将首版描述为完整 Ant Design Select。
- 常用组件优先级为 Select → Tabs → Card → Modal → Tag/Badge → Pagination → Table；每阶段验证、commit、push 并核对远端。

## Capabilities

### New Capabilities

- `select-control`: 原生单选、可搜索下拉选择及其值、焦点、窗口层和生命周期行为。

### Modified Capabilities

无。主规格目录暂无已归档能力；继承现有输入/窗口层/Theme 合同。

## Impact

公开 Select API、共用窗口服务的 Select 宿主及 scroll 分发入口、Theme Token 与对应测试、Gallery 和使用文档。风险集中于下拉点击与失焦顺序、受控值拒绝、IME 与导航优先级和窗口/祖先 clip；通过真实组件与原生窗口验证。
