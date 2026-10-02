# Design

## Context

动机见 proposal.md。checkbox.hpp 当前无 Group/value/ref；selection_component 已共享 Switch/Checkbox/Radio 的 pointer、focus、retained surface、标签语义。RadioGroup 是静态 options，Slider 已通过 WindowComponentServices::append_slot 保留并增加子树。Switch 046 提供有限 wave 和 ref 的生命周期基础。当前没有主 specs。

设计来源为锁定 [Checkbox](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/checkbox/Checkbox.tsx)、[Group](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/checkbox/Group.tsx) 与 [样式](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/checkbox/style/index.ts)。原生边界依据用户已明确的桌面收尾范围。

## Goals / Non-Goals

**Goals:** 保留旧 API，原生多选集合/动态选项与被动内容，组件主题隔离、有限反馈，完整共同合同和 Windows 窗口证据。

**Non-Goals:** 不实现 DOM/React/浏览器表单行为，不扩充 Radio API，不引入 GPU ABI/backend 分支。不上报未实测 Linux 结果。

## Decisions

1. CheckboxValue 使用 String/double/bool typed variant；数字要求 finite，1.0 与 true、String("1") 是不同值。CheckboxValues/vector 统一去重校验。CheckboxOption 提供 value、String label 与 own disabled；需要 Icon/丰富标签、单项回调时通过 retained CheckboxGroupContent 和 CheckboxLabel composition，避免以无比较函数对象制造 reactive options 的伪相等。
2. Group value/defaultValue 互斥；options 与手工 content 互斥。手工 Checkbox 通过最近祖先 Group 注册，skipGroup 可保留独立状态。Group 与单项禁用逻辑或。候选以当前注册顺序过滤/排序；无控组删除 option 后静默移除其选择，受控 value 不被内部改写。只在用户有效激活触发回调。
3. 动态 options 按 value 匹配保留 SelectionState；标签/disabled 使用内部 Signal 更新，新增通过 append_slot 事务挂载，失败先回滚新增子树再发布数据。最多 1024 项，验证和匹配有界 O(n²)，普通 checked/disabled 更新只触及相关保留对象。删除/重排不重执行无关内容，不使用 Virtual DOM。
4. Group 使用内部不可聚焦、不可激活的 interaction 顺序锚点，与现有 reorder_after 配合把动态子项插回组内 Tab 顺序；该锚点只有被动组命中范围，无视觉层或用户回调。布局以选项 order 保持 identity；不新增全局每帧排序。手工富内容不允许嵌套可激活子控件在 CheckboxLabel 内造成双重切换。
5. CheckboxRef 复用 owner-thread/lifetime/ref 绑定模式；autoFocus 只挂载时生效。激活先复制单项、group、onClick 回调与候选，再更新非受控状态及有限反馈，按单项 change → group change → click 执行，容忍回调销毁。RTL 使用 typed direction，镜像 indicator/标签，group 顺序保持数据顺序。
6. CheckboxThemeConfig 使用与其他组件一致的 seed/algorithm/token override，解析 controlInteractiveSize 对应的原生 indicator 大小、lineWidth/borderRadiusSM/paddingXS、颜色、font/label/focus 及 native wave 参数。新增 Theme identity/JSON 与 additive golden 字段；不要继续借用 Switch 或 Button 的局部主题作为 Checkbox 颜色来源。有限波纹只包围 indicator，复用共同 RoundedEffect/fragment 与三种取消合同。

## Risks / Trade-offs

- [动态 mount 中失败/销毁] → validate-before-publish、追加子树事务、重新查找 live state、ref reserve 回滚和清理测试。
- [受控候选或回调顺序错误] → typed value 种类/非有限拒绝、顺序/删除/不回写/回调销毁合同测试。
- [共享 selection 回退] → 同时运行 Radio、Switch、selection、Text、Button scene 与 Theme 测试。
- [布局/Tab 不一致] → 保留身份、order 与 interaction 锚点同步，并验证删除焦点/捕获后可继续导航。
- [主题临时配置在 MSVC Debug 下耗栈] → 延续 044 Prop 大值存储和 046 ThemeScope const-ref 创建，Gallery 样例保持分函数。

## Migration Plan

新增 API 保持单项旧调用；分平台通用状态/Group、ref/Theme/有限反馈、Gallery 集成与 Windows 原生证据阶段提交。构建使用 windows-msvc-headless Debug/Release 和 windows-msvc Debug/Release、Ninja Multi-Config/MSVC。Linux 真窗口在实际 Linux 机器单独验收，不重复共同合同。
