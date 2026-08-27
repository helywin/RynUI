## Why

RynUI 目前只在 `DefaultThemeSnapshot` 中零散保存 Text、Button 和 layout gap 数值，后续组件仍需要临时查询上游资料，容易因关注点不同而选择不同 Token、状态值和视觉实现；现有 Button 把 Ant Design 的 3px focus outline 与 1px 透明 offset 合成连续 4px 实心蓝色外扩区域，在 150% DPI 下尤其显重，已经暴露出“来源值、桌面映射值和渲染合同”没有分层的问题。现在需要建立一套离线、版本化、可机器校验的完整 Ant Design Design Token 基线，使后续开发与验收只依赖仓库内同一份事实源。

## What Changes

- 锁定 Ant Design `6.5.0` tag 与 commit `740ad964dc2397f33e40944367b0536a7314cc32`，把完整 Seed、Map、Alias 和 Component Token 名录、默认值、派生关系、上游来源与 SHA256 保存为仓库内 machine-readable manifest；每个上游 Token 都必须标记为 runtime、metadata、web-only、deprecated 或 unsupported，禁止静默遗漏。
- 提供面向开发者的中文 Design Token 规范，完整覆盖品牌与预设色板、语义色、文本与字体、尺寸/间距、control height、line/border、圆角、阴影、动效、透明度、z-index、断点、focus/disabled/loading/status 和 Component Token；日常组件开发不得再次联网重新解释数值，升级只能通过独立 OpenSpec change。
- 新增公开 typed Theme API 与内部确定性派生管线，用 C++ 类型表达 `Color`、logical length、typography、duration/easing、border、`ShadowLayer`/`ShadowList`、breakpoint 与 Token identity；实现 Seed → Default/Dark/Compact Algorithm → Map → Alias → Component Token、受控 override、继承与验证，不引入 React、DOM、CSS 字符串或 CSS-in-JS 运行时。
- 新增完整的阴影视觉基础：多层 outer/inset shadow、offset、blur、spread、颜色/透明度、圆角、裁剪、层级与 DPI logical-pixel 语义；扩展 Scene、GPU instance、DXIL/SPIR-V shader、dirty range 与诊断，使纯阴影颜色变化不触发布局或 scene topology 重建。
- 将现有 Text、Button、Flex 与 Space 迁移到统一 Token snapshot；明确区分 Ant Design upstream value 与 RynUI desktop adaptation。Button `focus-visible` 使用独立 typed focus Token，正确渲染 1px 透明 offset 与 3px 空心 outline，不再把两者合成连续 4px 蓝带，并锁定 keyboard-only、hover/active/disabled/loading 优先级与截图验收。
- 增加本地 Token catalog 示例与视觉验收窗口，集中展示 Default/Dark/Compact、色板、排版、间距、圆角、边框、三档 elevation、多层/方向/inset shadow、focus 与组件状态；平台通用算法和 headless contract 只在一个受支持平台验收，D3D12/DXIL 与 Vulkan/SPIR-V shader 行为按受影响平台分别保存证据。
- 非目标：本 change 不实现 Ant Design 的全部业务组件，不复制 React Props、CSS variable、DOM、CSS-in-JS 或浏览器 cascade；未实现组件的 Component Token 仍必须完整进入 catalog，并在组件落地时从该 catalog 映射，不得重新抓取上游定义。

## Capabilities

### New Capabilities

- `design-token-system`: 规定 Ant Design 6.5.0 完整 Token catalog、来源锁定、类型映射、完整性分类、离线文档与升级流程。
- `theme-runtime`: 规定 typed Seed/Map/Alias/Component Token、Default/Dark/Compact 派生、override/继承、响应式 Theme context 与最小失效合同。
- `shadow-rendering`: 规定多层 outer/inset shadow 的 typed value、Scene/GPU/shader 行为、DPI/clip/z-order、Button focus 与视觉验收合同。

### Modified Capabilities

<!-- 当前尚无已归档到 openspec/specs/ 的 capability；现有 Text/Button/Flex/Space 迁移由本 change 的新 Theme 合同覆盖。 -->

## Impact

- 新增公开 `ryn` Theme/Token value 与读取/override API，重构 `src/component/default_theme.*` 为可扩展 Token snapshot 和算法模块。
- 影响 Text、Button、Flex、Space 的视觉值来源、Button focus-visible 表现、Scene primitive、Quad/Shadow GPU instance、SDL GPU pipeline、HLSL shader、DXIL/SPIR-V 构建和上传诊断。
- 新增仓库内 Ant Design 6.5.0 manifest、中文 Token 参考文档、来源 hash/coverage contract、主题/阴影示例和视觉证据；不新增运行时第三方依赖。
- 主要风险是“完整”被误解为只复制当前组件用到的少量字段、Web CSS 值无法直接映射到 typed desktop value、多层 blur shadow 扩大 overdraw、主题切换错误触发 Layout/Structure，以及桌面 focus adaptation 与 upstream snapshot 混淆；这些风险由全量 manifest coverage、明确 support classification、性能计数和视觉矩阵约束。
