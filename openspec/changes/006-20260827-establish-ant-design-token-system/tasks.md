## 1. 锁定上游来源与完整 Token catalog

- [x] 1.1 定义 `design-tokens/ant-design/6.5.0/sources.lock.yaml` 与 catalog schema，记录 Ant Design `6.5.0` tag、commit `740ad964dc2397f33e40944367b0536a7314cc32`、license、受检 source path 和逐文件 SHA256；通过 schema test、hash verification 与篡改 fixture 验证普通离线构建可拒绝来源漂移
- [x] 1.2 实现只读取显式本地 Ant Design source directory 的 `tools/update_ant_design_tokens.py`，提取 Seed、Map、Alias、Component Token interface 与 default/derivation source；通过 pinned source fixture 验证工具不访问网络且同一输入生成 byte-identical 结果
- [x] 1.3 展开 preset palette、template key、deprecated/internal、CSS-only 与尚未实现组件字段，为每个条目写入稳定 identity、layer、category、value kind、source location、support classification、RynUI mapping 与 invalidation domain；通过 exact-set coverage test 证明 upstream 字段没有 missing/unclassified 项
- [x] 1.4 生成并提交 `catalog.yaml` 与中文 `docs/design-tokens.md`，完整覆盖颜色、字体、尺寸、间距、圆角、边框、阴影、层级、透明度、动效、断点与全部 Component Token；通过 catalog-to-doc coverage、稳定排序和重复 identity tests 验证文档与机器清单一致
- [x] 1.5 运行 catalog/schema/hash/doc generation tests、`openspec validate --all --strict --no-interactive` 与 `git diff --check`；以英文 `docs: lock Ant Design token catalog` 提交并推送本阶段相关文件，核对 remote SHA

## 2. Typed Token 基础值与生成边界

- [x] 2.1 在 `ryn` 命名空间定义 `Color`、logical length/offset、`Duration`、`CubicBezier`、`BorderToken`、`ShadowLayer` 与 `ShadowList` 等强类型值，拒绝 CSS shorthand、任意 string key、NaN/Infinity 和非法范围；通过 public value、equality、construction 与 compile-fail tests 验证 API 边界
- [x] 2.2 从 catalog 生成稳定 Token identity、value-kind metadata、component owner、support status 与 invalidation domain，保持 unsupported/web-only 条目可查询但不进入 runtime struct；通过生成文件 golden、增删字段和重复映射 tests 验证生成结果确定且无手写漂移
- [x] 2.3 将 Ant Design 多层 shadow 解析为有序 `ShadowList`，完整表达 outer/inset、x/y offset、blur、正负 spread、颜色和 alpha，并锁定 `boxShadow`、`boxShadowSecondary`、`boxShadowTertiary`、Button、Drawer、Popover/Card 与 Tabs overflow shadow；通过 normalized-value golden 和 layer-order tests 验证数值、方向与透明度未丢失
- [x] 2.4 为 Default Seed 锁定 primary/success/warning/error/info color、14px font、1px line、6px radius、4px size unit、32px control height、z-index、opacity 与 motion 基值；通过 source-reference contract test 逐字段核对 Ant Design 6.5.0，而不是依赖运行时网络查询
- [x] 2.5 运行 typed value、generated metadata、shadow normalization、public header isolation 与 `git diff --check`；以英文 `feat: add typed design token values` 提交并推送本阶段相关文件，核对 remote SHA

## 3. Seed、Map、Alias 与 Component Algorithm

- [x] 3.1 实现 immutable Seed input、palette/neutral alpha compositing、font/size/control/radius/motion Map derivation 与 Alias projection，明确颜色空间和舍入顺序；通过锁定 Default golden 的逐字段 parity test 验证结果
- [x] 3.2 实现 `ThemeAlgorithm::Dark` 与 `ThemeAlgorithm::Compact`，允许按声明顺序与 Default 组合；通过 Default、Dark、Compact、Dark+Compact 和 Compact+Dark golden 验证确定性及顺序语义
- [x] 3.3 实现 typed Seed/Alias override 与 Component Token override，锁定 `inherit` 和 override precedence；通过 brand Seed、semantic color、font/spacing、Button token 与非法类型 fixtures 验证只覆盖目标字段
- [x] 3.4 实现默认关闭的 component algorithm，显式开启时只在目标 component scope 重新派生；通过多个 component owner、nested override、未开启回归和 upstream golden 验证不会隐式改变其他组件
- [x] 3.5 增加 source lock version、algorithm chain、resolved values、identity 与 impact metadata 的 `ThemeSnapshot` 序列化/诊断视图；通过相同输入 byte-identical、不同输入 diff 和错误输入原子失败 tests 验证 snapshot 可复现
- [x] 3.6 在任一受支持平台运行 Algorithm/golden/override 全套平台通用 tests 并记录实际 OS、compiler 与 preset，再运行 `git diff --check`；以英文 `feat: derive Ant Design theme snapshots` 提交并推送本阶段相关文件，核对 remote SHA

## 4. Theme scope、继承与细粒度失效

- [x] 4.1 定义公开 `ThemeConfig`、`ThemeProps`、typed content slot 与 `ryn::Theme`，使用 reactive `Prop<ThemeConfig>` 且不暴露 runtime、SDL3、GPU、CSS variable 或通用 style map；通过 public consumer、header isolation 与 compile-fail tests 验证边界
- [x] 4.2 在 Host 注入 Default snapshot，并实现 nested Theme 的默认继承与 `inherit=false` 重置；通过无 Theme、单层/多层 scope、sibling isolation、异常 slot、destroy/reuse 和跨线程失败 tests 验证生命周期
- [x] 4.3 让 typed Token accessor 在 reactive scope 记录 identity subscription，Theme 更新对 immutable snapshot 做逐字段 diff；通过订阅/未订阅组件、等值切换、nested override 和 stale identity tests 证明普通更新不重跑无关 Component
- [x] 4.4 按 catalog invalidation domain 将变化映射到 Paint/Material、Geometry、Text、Measure/Layout、HitTest 与 Animation，禁止 Theme update 触发 Structure rebuild；通过每类代表 Token、混合变化、错误更新回滚和 dirty counter tests 验证最小失效
- [x] 4.5 增加 Theme generation、changed identity、subscriber、dirty phase、snapshot reuse 与 allocation 诊断；通过 steady-state update benchmark 证明等值更新不请求帧、局部颜色更新不测量且稳定订阅不分配
- [x] 4.6 运行 Theme public/runtime、Component lifecycle、Dirty、frame scheduler、allocation 与 `git diff --check`；以英文 `feat: add reactive theme scopes` 提交并推送本阶段相关文件，核对 remote SHA

## 5. RoundedEffect 模型、Scene 与 CPU reference

- [x] 5.1 定义内部 `RoundedEffectInstance`、effect kind、logical rounded rect、offset、blur、spread、outline width/offset、color/opacity、translation 与 clip identity；通过 value/layout contract tests 覆盖 outer、inset、negative spread、空 shadow 和非法参数
- [x] 5.2 实现 rounded-rect signed-distance CPU reference，outer shadow 采用 spread 后 `sigma = blur / 2` coverage，inset 反向计算，outline 用两个 SDF boundary 相减；通过解析点、golden mask 和 property tests 验证无硬边、透明 gap 与 corner symmetry
- [x] 5.3 实现包含 offset、positive spread、`3*sigma` 和 antialias guard 的 tight effect bounds，保持 negative spread、translation、ancestor clip 与 surface content clip 语义；通过极值、DPI scale、部分/完全裁剪和无隐式 child clip tests 验证不裁切字母或阴影边缘
- [x] 5.4 将有序 `ShadowList` 展开为 retained effect child，保证 outer 位于所属 surface fill 前、inset 位于 surface 内且跨组件服从 scene paint order；通过 nested/sibling、clip、destroy/reuse 和 Button-like composition tests 验证顺序
- [x] 5.5 为 effect geometry/material 建立独立 store、dirty range、compact/cull 和诊断，不扩大普通 `QuadInstance`；通过 sparse update、capacity reuse、空 shadow、1k effect benchmark 与 idle tests 验证 retained 性能边界
- [x] 5.6 运行 effect math、bounds、scene order、clip、store/benchmark 与 `git diff --check`；以英文 `feat: add retained rounded effects` 提交并推送本阶段相关文件，核对 remote SHA

## 6. 共享 HLSL 与 GPU effect pipeline

- [ ] 6.1 新增锁定的 `rounded_effect.hlsl` 与 reflection/instance-layout contract，从同一源生成 DXIL 和 SPIR-V artifact；通过 shader source hash、reflection、artifact freshness 与 deployment tests 验证格式没有手工分叉
- [ ] 6.2 实现 multi-layer outer/inset shadow 与 hollow outline shader，锁定 straight-color 输入、alpha coverage 和 pipeline blend 约定；通过 CPU reference 对照、透明/半透明背景、重叠 layer 与 alpha edge fixtures 验证不会异常加深
- [ ] 6.3 增加独立 effect GPU buffer、pipeline、batching、clip/cull、partial upload 和 draw submission，普通无 effect frame 不绑定该 pipeline；通过 fake backend、buffer growth/failure、dirty range、zero-effect 与 renderer cleanup tests 验证资源收口
- [ ] 6.4 将 logical-to-device conversion 应用于 geometry、blur、spread、offset、outline 与 antialias，保持不同 display scale 下 logical 外观一致；通过 100%/150%/200% simulated metrics 和 pixel-bound contract tests 验证非整数 DPI 不吞边或错误增厚
- [ ] 6.5 在一个受支持平台运行 shader-independent SDF/reference、reflection、Scene/GPU contract 与 benchmark tests 并记录实际平台/preset，再运行 `git diff --check`；以英文 `feat: render rounded shadows and outlines` 提交并推送本阶段相关文件，核对 remote SHA

## 7. 现有组件迁移与 Button 焦点修正

- [ ] 7.1 将 Text 的 font family/size/weight/line-height/color、Button 的尺寸/颜色/边框/圆角/阴影/focus、Flex/Space 的 gap 从 `DefaultThemeSnapshot` 常量迁移到 resolved Theme accessor；通过默认 snapshot compatibility test 证明未覆盖时现有 logical layout 不变
- [ ] 7.2 为 Text、Button、Flex 与 Space 建立精确 Token subscription，分别验证颜色只更新 material、字体更新重塑/测量、Button effect 更新 geometry/paint、gap 更新目标 layout，且 content slot 和 scene identity 不重建
- [ ] 7.3 将 Button focus-visible 改为 1 logical px 透明 offset 后的 3 logical px hollow ring，保留 Ant Design `lineWidthFocus` 数值；通过 CPU/GPU geometry、中心/边界 sample、150% DPI 与 focus modality tests 证明不再绘制连续 4px 蓝带
- [ ] 7.4 接入 Button default/primary/danger 及 hover/active/disabled/loading shadow token，确保 disabled/loading/state precedence 与 shadow/focus 可同时表达；通过完整状态矩阵、nested Theme override 和 reactive transition tests 验证视觉层不互相覆盖
- [ ] 7.5 删除重复组件视觉常量并保留最小 migration adapter，使用 forbidden-pattern/source contract 防止新稳定组件绕过 Theme/Component Token；通过 public example、header leak 和旧 adapter 零新增引用 tests 验证迁移边界
- [ ] 7.6 运行 Text/Button/Flex/Space、Theme invalidation、focus/interaction、layout、scene、renderer 与 `git diff --check`；以英文 `feat: migrate components to theme tokens` 提交并推送本阶段相关文件，核对 remote SHA

## 8. Token Gallery 与平台通用验收

- [ ] 8.1 新增公开 Token Gallery DSL 示例，以稳定 test id 展示 semantic/preset colors、text hierarchy、font scale、spacing/control height、radius/border、Default/Dark/Compact 与 nested override；通过 source contract 证明示例只使用公开 API
- [ ] 8.2 在 Gallery 展示三档 elevation、Button default/primary/danger shadow、Drawer 四向 shadow、Popover/Card、Tabs inset overflow，以及 focus-visible/hover/active/disabled/loading；通过 catalog identity coverage test 证明阴影和交互状态没有漏项
- [ ] 8.3 实现宽/窄 viewport、主题/algorithm/brand Seed 切换与可观测 counters；通过 headless frame test 验证布局可完整访问、Theme content 不重跑、局部 dirty/upload 生效且 idle 不持续 submit
- [ ] 8.4 增加 effect geometry、layer order、focus gap/ring、Theme update counter、snapshot 与 benchmark 的平台通用 acceptance contract；只选择一个受支持平台运行并在 evidence 中记录 OS、compiler、preset、catalog hash 与退出码，不要求另一平台重复
- [ ] 8.5 定义 Windows 与 Linux 独立 GPU evidence schema，要求 display scale、window system、GPU/driver、shader format/hash、截图、effect/layer/upload/draw/submit counters 和退出码，并通过交叉平台 identity rejection test 验证证据不可相互代替
- [ ] 8.6 在选定平台运行本 change 的完整平台通用 unit/headless/contract/benchmark tests、OpenSpec strict validate 与 `git diff --check`；以英文 `feat: add design token gallery` 提交并推送本阶段相关文件，核对 remote SHA

## 9. Windows MSVC、D3D12 与 DXIL 验收

- [ ] 9.1 使用 `windows-msvc` preset clean configure 并完成 Debug/Release build，运行 Windows/D3D12/DXIL 专属 shader reflection、artifact deployment、renderer startup/teardown 与 DPI integration tests；核对 Ninja Multi-Config、MSVC x64 和锁定 shader source，不重复要求已在第 8 节完成的平台通用测试
- [ ] 9.2 在 Windows/MSVC/D3D12/DXIL 真实窗口以 100%、150% 与 200% display scale 运行 Token Gallery，实际切换 Default/Dark/Compact、brand Seed、hover/active/focus-visible/disabled/loading 并正常退出；保存各缩放截图、GPU/driver、DXIL hash、退出码和诊断计数
- [ ] 9.3 人工核对三档 elevation、多层/方向/inset shadow 没有硬边、异常加深或裁切，并确认 Button 是 1 logical px 透明 gap 加 3 logical px hollow ring、字体/圆角边缘清晰且左右留白一致；运行 Windows evidence passed contract
- [ ] 9.4 运行 Windows 专属 integration CTest、shader/lock/license、evidence、未跟踪依赖与 `git diff --check`；以英文 `test: validate Windows theme rendering` 提交并推送 Windows evidence/截图，核对 remote SHA，不修改 Linux 清单

## 10. Linux Vulkan 与 SPIR-V 验收

- [ ] 10.1 在原生 Linux 使用 `linux-gcc` 或 `linux-clang` preset clean configure 并完成 Debug/Release build，运行 Linux/Vulkan/SPIR-V 专属 shader reflection、artifact deployment、renderer startup/teardown 与 display-scale integration tests；核对 Ninja Multi-Config、标准 C++20 和锁定 shader source，不重复要求平台通用测试
- [ ] 10.2 在 Linux/Wayland/Vulkan/SPIR-V 真实窗口于至少两个可用 display scale 运行 Token Gallery，实际切换 Default/Dark/Compact、brand Seed、hover/active/focus-visible/disabled/loading 并正常退出；保存 window system、截图、GPU/driver、SPIR-V hash、退出码和诊断计数
- [ ] 10.3 人工核对三档 elevation、多层/方向/inset shadow 没有硬边、异常加深或裁切，并确认 Button 的透明 gap、hollow ring、系统默认字体、圆角与留白；运行 Linux evidence passed contract，不能以 Windows 结果替代
- [ ] 10.4 运行 Linux 专属 integration CTest、shader/lock/license、evidence、未跟踪依赖与 `git diff --check`；以英文 `test: validate Linux theme rendering` 提交并推送 Linux evidence/截图，核对 remote SHA，不修改 Windows 清单

## 11. Change 收口

- [ ] 11.1 更新 `docs/architecture.md` 的长期 Theme/Token/Shadow 决策与文档入口，不复制完整 catalog 或 README 产品介绍；通过 documentation link、forbidden duplication 与 `git diff --check` 验证 AGENTS、README、architecture、generated token docs 和 OpenSpec 职责清晰
- [ ] 11.2 在准备 archive 时运行 `openspec doctor --json`、`openspec validate --all --strict --no-interactive` 与 `git diff --check`，确认 catalog coverage 为 exact、平台通用验收已有且只有一份有效记录、Windows 与 Linux 专属 GPU 清单分别完成、所有 commit 已推送且 worktree clean；本项不替代任一平台专属验收
