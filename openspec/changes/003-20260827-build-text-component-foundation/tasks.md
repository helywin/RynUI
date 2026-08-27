## 1. Reactive Prop 值边界

- [x] 1.1 在公开 header 中实现拥有静态 `T` 或 type-erased `Binding<T>` reader 的 `Prop<T>`，支持静态值、任意 `Signal<T, Equal>` 与临时 `Binding<T>`，通过 compile/run tests 覆盖 custom equality、临时 Props/Binding 生命周期和禁止裸引用借用
- [x] 1.2 实现内部 `connect_prop`：静态值只应用一次、响应值进入 binding phase、相等结果不下发，并通过 Observer/Dirty/frame request 计数测试证明三种来源共享一个字段更新路径且重复相同值不扩大失效
- [x] 1.3 把 Prop API 接入 `rynui.hpp`，扩展 public-header isolation/forbidden include 与 compile-fail contract，证明 consumer 只需 C++20/RynUI public target，且 Prop 不暴露 Observer、Node、Layout、FreeType、HarfBuzz 或 SDL3 类型
- [x] 1.4 运行 Prop/Reactive 全部测试、`linux-gcc` 与 `linux-clang` Debug build/CTest、`git diff --check`；以英文 `feat: add reactive prop values` 提交本阶段相关文件

## 2. Component Host、identity 与 typed composition

- [x] 2.1 建立内部 component slot + generation registry，使 record 持有 Scope、root Node、类型 state 与声明顺序，通过 create/find/destroy/reuse tests 证明 stale identity 不访问复用 slot
- [x] 2.2 实现 UI owner-thread active mount stack 与公开 typed content closure 入口，使 `ryn::Text` 类 DSL 可以取得当前父级而不公开 `MountContext`；通过嵌套 slot、兄弟顺序、无 active Host、错误线程、异常退出和重入测试验证 stack 始终恢复
- [x] 2.3 实现 Host/component dispose 顺序：先停止 Scope/Prop Observer，再移除类型资源与 Scene range，最后销毁 Node 子树并推进 generation；通过重复 dispose、父子清理和销毁后 Signal write 测试证明没有重复释放、帧请求或 stale callback
- [x] 2.4 增加 typed slot compile contracts，证明合法 content 按声明顺序挂载、不支持的 prefix/footer 等 slot 在类型层不存在，且普通 Prop 更新不重新执行 content closure或改变兄弟 identity
- [x] 2.5 运行 component/Scope/Node 生命周期测试、GCC/Clang Debug build/CTest 与 `git diff --check`；以英文 `feat: add component composition host` 提交本阶段相关文件

## 3. LayoutStyle 与 intrinsic measurement

- [x] 3.1 在公开 header 中实现 strong logical length/auto 值、四边 margin 与 `LayoutStyle` typed builder，首批字段覆盖 width、height、min/max width、min/max height；通过 API/compile tests 验证字段可接收对应 `Prop<T>` 且不存在颜色、字体、padding、background 或通用 Modifier 入口
- [x] 3.2 在 retained Node/layout metadata 中实现无 wrapper 的外部 constraint 与 margin 编译、原子校验和 Dirty 分类，通过负数、NaN、min>max、固定尺寸、min/max clamp、margin 与 Node count 测试证明非法更新不改状态且不创建额外 Node
- [x] 3.3 为 Layout Engine 增加以 Node generation 为 key 的平台无关 intrinsic measure adapter 注册/注销与 revision cache，通过自定义 leaf tests 验证 constraint 传递、相同 revision 复用、递归测量 fail-fast 和 stale Node 不调用旧 adapter
- [x] 3.4 把 reactive LayoutStyle 更新接入最小失效：有效 content constraint 变化触发 Measure/Layout，纯 margin 变化只更新 place/geometry；通过计数测试证明不重新挂载组件、不改 Text Material，content width 未变时不重复 measure
- [ ] 3.5 运行 layout/component/public API 全部测试、Windows MSVC Debug 与 Linux GCC/Clang Debug build/CTest、`git diff --check`；以英文 `feat: add public layout style` 提交本阶段相关文件

## 4. 多 Text 共享 scene service

- [x] 4.1 将单 `TextRenderController` 泛化为 Host 级 Text scene service 与 per-Text generation-checked record，保留独立 content/tone/layout revision、TextState、measurement cache、Node 和 Scene range；通过两个及以上 Text 的 create/sync/order tests 验证声明顺序稳定
- [x] 4.2 实现多个 Text 共享 Font Runtime、GlyphAtlas、GlyphScene 与 renderer upload plan，通过重复 glyph tests 证明第二个 Text 命中现有 raster/atlas entry，不重复 texture upload
- [x] 4.3 实现 Text range 删除、局部 compact/remap 与 generation 校验，通过销毁首个/中间/末尾 Text、slot 复用和后续更新测试证明 surviving Text identity、atlas UV、draw order 与 dirty range 正确
- [x] 4.4 分离 content、tone、constraint 与 placement dirty path：content 执行目标 shape/measure/atlas/instance，tone 只更新目标 Material，constraint 只重新 measure/layout，placement 只更新 geometry；通过逐 Text 诊断计数和不相邻 dirty range tests 证明不扩大为全量 upload
- [ ] 4.5 运行 Font/Text/Glyph/renderer 与多 Text 集成测试、MSVC/GCC/Clang Debug build/CTest、`git diff --check`；以英文 `feat: support multiple text components` 提交本阶段相关文件

## 5. 公开 TextProps 与 ryn::Text

- [ ] 5.1 建立内部 immutable Default Theme snapshot，锁定 Ant Design 6.5.0 常规 14 logical-pixel 正文、line height 与 primary/secondary/disabled alias color，通过 token contract test 验证字段和值，且公开 API 不提供 Theme override、任意颜色/字号/字体或 PrimitiveStyle
- [ ] 5.2 在公开 header 中实现 `TextTone`、拥有 `Prop<String>`/`Prop<TextTone>`/`LayoutStyle` 的 typed `TextProps` builder 与 `ryn::Text`，提供复用同一 Props 路径的 `String`/`u8` literal convenience overload；通过 API、CJK literal、Signal/Binding 与 compile-fail tests 证明不保存 `StringView` 或 narrow literal
- [ ] 5.3 实现 Text mount adapter：创建 component record/Node、连接 content/tone/layout Prop、注册 intrinsic measure，并把 Host Theme 与 Text scene service 接到既有 shaping/Glyph pipeline；通过首次挂载、兄弟 Text、错误线程和 dispose tests 验证 owner-thread 与生命周期
- [ ] 5.4 接通 shaped measurement、Node bounds、clip/translation 与 Glyph instance placement，通过自然宽度、有限宽 CJK/Latin 换行、margin、resize 和 translation tests 证明按 glyph metrics 测量、cluster 完整且移动不 shaping
- [ ] 5.5 扩展 `rynui.hpp`、public target 和 header isolation/leak checks；通过只包含公开头的 consumer 编译证明 component/Text API 不出现内部 Host、NodeId、layout model、Font/HarfBuzz/FreeType/Scene/GPU/SDL3 类型
- [ ] 5.6 运行 Text component、Prop、Layout、多 Text 与 public API 全部测试、MSVC/GCC/Clang Debug build/CTest、`git diff --check`；以英文 `feat: add public text component` 提交本阶段相关文件

## 6. Windows 真实窗口组件验收

- [ ] 6.1 将 Latin/CJK 文本示例迁移为公开 `ryn::Text` content DSL，由内部 application host 启动 primary、secondary、disabled 和多个共享 glyph 的 Text；通过 source/contract test 验证示例不直接构造 TextState、GlyphAtlas、GlyphScene 或 TextRenderController
- [ ] 6.2 为示例增加可控 content、tone、width、margin 与 resize 更新及 mount、Prop update、shape、measure、layout、atlas、instance、draw、submit、idle 计数，通过 headless event/clock tests 验证每种更新的最小失效关系和稳定后停止 submit
- [ ] 6.3 使用 `windows-msvc` preset clean configure，完成 Debug/Release build 与完整 CTest；在 D3D12/DXIL 真实窗口运行示例并正常关闭，保存 display scale、截图、driver、shader format、退出码和全部计数证据，人工核对 14px Latin/CJK、三种 tone、换行、margin 与共享 glyph
- [ ] 6.4 运行 Windows evidence contract、public dependency leak 和 `git diff --check`；以英文 `test: validate Windows text components` 提交本阶段相关文件，不把 headless/build 结果描述为其他平台真实窗口通过

## 7. Linux 验收与 change 收口

- [ ] 7.1 使用 `linux-gcc` preset clean configure，完成 Debug/Release build 与完整 CTest，核对 Ninja Multi-Config、标准 C++20、BUNDLED FreeType/HarfBuzz/字体和 SPIR-V shader 仍来自锁定来源
- [ ] 7.2 使用 `linux-clang` preset完成 Debug configure/build/CTest，核对 RynUI 自有 Prop/component/Layout/Text/renderer target 使用 `-std=c++20` 而非 GNU extensions；环境缺 Clang 时保持未完成并记录可复现阻塞
- [ ] 7.3 在 Linux/GCC/Vulkan/SPIR-V 真实窗口运行与 Windows 相同的公开 Text DSL 示例，触发 content、tone、width、margin 与 resize 更新并正常退出；保存 display scale、截图、driver、退出码和计数证据，核对三种 tone、局部更新、共享 atlas、稳定 draw order 与 idle
- [ ] 7.4 运行 `BUNDLED|SYSTEM` dependency contract、public-header isolation/leak、shader/lock/license 与未跟踪字体检查，证明 003 不新增第三方 library、运行时字体扫描、submodule 或 system-first fallback
- [ ] 7.5 运行 Windows/Linux 对应全量 CTest 和 evidence checks、`openspec doctor --json`、`openspec validate --all --strict --no-interactive` 与 `git diff --check`；只在任务 1–7 的自动测试和真实窗口证据均满足后勾选并记录最终验收摘要
- [ ] 7.6 确认 worktree 只包含本 change 相关文件，README、AGENTS、architecture 与 OpenSpec 职责未混写；以英文 `test: validate cross-platform text components` 提交最终验收阶段，不主动 push 或 archive
