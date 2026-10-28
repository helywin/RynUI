# Proposal

## Why

RynUI 已有原生 Select、Tree 和交互弹层，但用户尚不能在紧凑表单字段中选择层级数据。新增复用这些能力的单选 TreeSelect，使目录、项目和组织节点选择形成可运行流程。

## What Changes

- 新增 typed `TreeSelectProps`、固定层级 `TreeSelectItem` 与 `TreeSelectRef`；字符串 key 为值，空字符串表示未选。
- 支持受控／非受控 value、open、展开状态，响应式标题、disabled、visible、clear、size、variant、status 与有限高度列表。
- 组合 retained 原生 Input、Popover 和 Tree，支持真实指针、键盘、焦点恢复、边界关闭及 Form String 绑定与 reset。
- 新增独立 TreeSelect Theme 配置、继承、算法、identity、dirty 和序列化；维持已有 token 和默认栈约束。
- 首版不包含多选／checkbox、搜索、动态拓扑、异步加载、虚拟化、富节点／自定义 popup、动画、RTL、Web 或辅助技术。Gallery 与分平台原生验收不在本 change 内。

## Capabilities

### New Capabilities

- `native-tree-select`: 固定层级单值选择、交互生命周期、表单组合及独立 Theme 合同。

### Modified Capabilities

无。当前 `openspec list --specs` 无已同步主 spec；本变更新增 delta，不改历史 change。

## Impact

影响公开 TreeSelect 头文件、组件组合 host、窗口 participant 注册、Theme 全链、CMake、专项测试和使用文档。无新增第三方依赖，无 renderer／GPU ABI、RC 资源或系统字体行为变更。风险集中于受控 authority、组合挂载异常、焦点回调重入及弹层尺寸／裁剪，需实际 HEADLESS 回归并独立 Astra 审核。
