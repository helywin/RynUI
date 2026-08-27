## 1. Flex line、wrap、gap 与对齐算法

- [x] 1.1 扩展内部 Flex layout value，增加 wrap、justify、align、main/cross gap 与严格非负有限校验；通过 model equality、非法值、空容器和现有 horizontal/vertical regression tests 验证默认行为兼容
- [x] 1.2 实现按有限主轴约束的 greedy line formation 与可复用 item/line scratch；通过单行、多行、单个超限 child、无限约束、margin 和稳定 child count allocation tests 验证 line break 确定且稳态不分配
- [x] 1.3 实现 start/center/end/space-between/space-around/space-evenly justify 与 start/center/end/stretch align；通过单/多 item、正/零自由空间、显式 cross size、min/max 和 fractional constraints 重复运行 tests 验证 bounds 稳定
- [x] 1.4 接通双轴 gap、wrapped line cross placement 与 logical coordinate 诊断，通过 8/16/24/custom gap、窄宽 viewport、嵌套 Flex 和 HitTest bounds tests 证明首尾无额外 gap 且 geometry 同步
- [x] 1.5 运行 LayoutEngine、Node、Text intrinsic、Button content、HitTest 与 allocation 自动测试和 `git diff --check`；以英文 `feat: add wrapped flex layout` 提交并推送本阶段相关文件，核对 remote SHA

## 2. Flex child LayoutStyle 与自由空间分配

- [ ] 2.1 扩展公开 `LayoutStyle` 与内部 `ExternalLayoutStyle`，增加 reactive grow、shrink、basis、align-self、order，保持字段只表达外部布局；通过 public API、adapter、非法负数/NaN/无穷值和非 Flex parent regression tests 验证边界
- [ ] 2.2 实现 hypothetical main size、正自由空间 grow、负自由空间 `shrink * basis`、min/max freeze 与确定余数分配；通过 1:2 权重、零权重、多轮 clamp、固定 basis、Text 最终宽度重测和极窄约束 tests 验证结果
- [ ] 2.3 实现 `(order, declaration ordinal)` 稳定 layout order 与 align-self override，通过响应式 order、相同 order、destroy/reuse、paint/HitTest/focus order 保持声明顺序 tests 验证不扩大为 Structure dirty
- [ ] 2.4 接入 flex child Prop 的局部 Measure/Layout/Geometry/HitTest dirty 与 sibling isolation 诊断，通过普通更新不重挂 child、不重建 scene topology、错误线程 fail-fast 和销毁后 Signal write tests 验证 retained 生命周期
- [ ] 2.5 运行 LayoutStyle、Flex allocation、Component lifecycle、Pointer/Focus 与 benchmark 自动测试和 `git diff --check`；以英文 `feat: add flex item layout properties` 提交并推送本阶段相关文件，核对 remote SHA

## 3. 共享 gap value、Theme token 与公开 Flex

- [ ] 3.1 增加 `SpaceSize`、`LayoutGap`、`FlexJustify`、`FlexAlign` 等公开 typed value，并锁定 Default Theme 的 Small/Middle/Large gap 为 8/16/24 logical pixels；通过 source-reference、equality、custom 双轴值、invalid construction 和 token contract tests 验证
- [ ] 3.2 实现拥有 vertical/wrap/justify/align/gap/`LayoutStyle` reactive `Prop<T>` 的 `FlexProps`、专用 typed content 与 `ryn::Flex` 声明入口；通过 public API/compile-fail tests 拒绝字符串、内部 enum、foreign slot 与通用视觉字段
- [ ] 3.3 实现 Flex mount adapter：创建无视觉 root、连接 LayoutStyle/Props、执行一次 content、保留 direct child 与逆序 cleanup；通过异构/nested/empty content、异常 slot、无 active Host、错误线程、destroy/reuse 和销毁后更新 tests 验证生命周期
- [ ] 3.4 将 Props 更新映射到最小 layout phase，证明 justify/align 可复用 measurement，gap/direction/wrap 只重测目标子树，普通更新不重跑 content、不改变 scene/interaction identity 且 idle 后停止 submit
- [ ] 3.5 从 `rynui.hpp` 导出 Flex API，通过只依赖 public target 的 consumer 编译/链接和 header isolation/leak checks 证明不出现 Node、LayoutEngine、SDL3、GPU 或 Theme 内部类型
- [ ] 3.6 运行 Theme、Flex public/component、Layout、Text/Button composition、Dirty 与 public dependency 自动测试和 `git diff --check`；以英文 `feat: add public flex component` 提交并推送本阶段相关文件，核对 remote SHA

## 4. 公开 Space 容器

- [ ] 4.1 实现拥有 vertical/wrap/align/size/`LayoutStyle` reactive `Prop<T>` 的 `SpaceProps`、专用 typed content 与 `ryn::Space`；默认解析为 horizontal/no-wrap/Small，通过 public API/compile-fail tests 拒绝 baseline、separator、Compact、foreign slot 与任意样式字段
- [ ] 4.2 实现 Space mount adapter 和受限 Flex policy，不增加 wrapper Node、不使 grow/shrink/basis/order 生效；通过 horizontal/vertical、三种 preset/custom 双轴 gap、nested/empty content、direct child identity 与非 Flex item regression tests 验证
- [ ] 4.3 实现 Space wrap 与 start/center/end align，通过宽窄 viewport 往返、CJK/Latin Text、不同尺寸 Button、margin 和 sibling composition tests 证明等距排列且 child 不销毁/重挂
- [ ] 4.4 接入 Space 响应更新、局部 dirty、cleanup 与诊断，通过 size/orientation/wrap transition、错误更新原子性、Scope dispose、steady-state allocation 和 idle frame tests 验证 retained 行为
- [ ] 4.5 从 `rynui.hpp` 导出 Space API，运行 Space public/component、Layout、Text/Button composition、HitTest/Focus、dependency leak 与 `git diff --check`；以英文 `feat: add public space component` 提交并推送本阶段相关文件，核对 remote SHA

## 5. 平台无关示例与 headless 验收

- [ ] 5.1 新增公开 Flex/Space DSL 示例，以嵌套容器组织 Text/Button，并通过 Button 响应切换 direction、wrap、justify、align、gap、grow 与 order；source contract 必须证明示例不直接构造 ComponentHost、Node、LayoutEngine 或 SDL event
- [ ] 5.2 增加多 viewport headless frame tests，覆盖宽/窄 line break、双轴 gap、grow/shrink、align/order、pointer/keyboard 激活后布局更新、identity/dirty 计数和 idle；锁定 content closure 只执行一次且 scene topology 不因普通 Props 变化重建
- [ ] 5.3 增加平台独立 layout evidence schema，要求 preset、compiler、window system/display scale、viewport、line/layout/scene/submit 计数和宽/窄截图路径；Linux 与 Windows 使用独立文件且交叉平台 identity 必须被合同拒绝
- [ ] 5.4 运行 Flex/Space example source contract、headless frame integration、evidence schema、public dependency、未跟踪依赖与 `git diff --check`；以英文 `feat: add responsive layout demo` 提交并推送本阶段相关文件，核对 remote SHA

## 6. Linux 验收清单

- [ ] 6.1 使用 `linux-gcc` preset clean configure，完成 Debug/Release build 与完整 CTest，核对 Ninja Multi-Config、`-std=c++20`、BUNDLED 依赖和 Vulkan/SPIR-V 锁定来源，保存 Linux 构建结果
- [ ] 6.2 使用 `linux-clang` preset 完成 Debug configure/build/CTest，核对 Flex/Space/LayoutStyle target 使用标准 C++20 而非 GNU extensions；环境缺 Clang 时保持未完成并记录可复现阻塞
- [ ] 6.3 在原生 Linux Wayland/GCC/Vulkan/SPIR-V 真实窗口运行公开 layout 示例，完成窗口宽窄调整、跨不同缩放输出、Button 切换 direction/wrap/justify/align/gap/grow/order 与正常退出；保存 window system、display scale、宽/窄截图、退出码和诊断计数，不以强制 X11 结果代替 Wayland 验收
- [ ] 6.4 人工核对 Flex/Space 的 horizontal/vertical、wrap、Small/Middle/Large/custom gap、对齐、grow/shrink、order、CJK/Latin 与 Button 命中位置；运行 Linux evidence passed contract、public dependency、shader/lock/license 与未跟踪依赖检查
- [ ] 6.5 运行 Linux 完整 CTest、OpenSpec strict validate 与 `git diff --check`；以英文 `test: validate Linux flex and space layouts` 提交并推送 Linux 清单/evidence/截图，核对 remote SHA，不修改 Windows 清单

## 7. Windows 验收清单

- [ ] 7.1 使用 `windows-msvc` preset clean configure，完成 Debug/Release build 与完整 CTest，核对 Ninja Multi-Config、MSVC x64、标准 C++20、BUNDLED 依赖和 D3D12/DXIL 锁定来源，保存 Windows 构建结果
- [ ] 7.2 在 Windows/MSVC/D3D12/DXIL 真实窗口运行公开 layout 示例，完成窗口宽窄调整、不同系统缩放输出、Button 切换 direction/wrap/justify/align/gap/grow/order 与正常退出；保存 display scale、宽/窄截图、退出码和诊断计数
- [ ] 7.3 人工核对 Flex/Space 的 horizontal/vertical、wrap、Small/Middle/Large/custom gap、对齐、grow/shrink、order、CJK/Latin 与 Button 命中位置；运行 Windows evidence passed contract、public dependency、shader/lock/license 与未跟踪依赖检查
- [ ] 7.4 运行 Windows 完整 CTest、OpenSpec strict validate 与 `git diff --check`；以英文 `test: validate Windows flex and space layouts` 提交并推送 Windows 清单/evidence/截图，核对 remote SHA，不修改 Linux 清单

## 8. Change 收口

- [ ] 8.1 在准备 archive 时运行 `openspec doctor --json`、`openspec validate --all --strict --no-interactive` 与 `git diff --check`，确认所有独立验收条目已完成、worktree 只包含本 change 相关文件且 README、AGENTS、architecture 与 OpenSpec 职责未混写；本项不替代任何平台验收
