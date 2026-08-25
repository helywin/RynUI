## 1. C++ 工程与测试基线

- [x] 1.1 创建 C++20/CMake 目录和分层 targets，提供 `rynui` public facade 与 `include/ryn/` 核心公开头文件，通过 configure 验证 target 依赖方向正确，并确认没有提前暴露通用组件视觉 `Modifier` 或与 typed Props/slots 冲突的 API
- [x] 1.2 创建基于 hidden base preset 的 `CMakePresets.json`，以 `Ninja Multi-Config` 管理 `windows-msvc`、`linux-gcc` 和 `linux-clang` configure preset 及对应 Debug/Release build/test preset；Windows configure 必须拒绝非 MSVC 工具链
- [x] 1.3 建立不依赖 SDL3 的 CTest 测试入口和最小 `ryn` API smoke test，并使用 `windows-msvc` preset 在 Windows 上运行 Debug build 与 `ctest --output-on-failure`
- [x] 1.4 补充 Visual Studio Developer Environment、跨平台 preset、生成物忽略规则和依赖边界说明，运行 `git diff --check` 后使用英文 commit message 提交本阶段

## 2. SDL3 平台与 GPU 生命周期

- [x] 2.1 建立集中依赖 lock 与 license 记录，固定 SDL3 和 `SDL_shadercross` 的不可变 source URL 与 SHA256；实现只接受 `BUNDLED|SYSTEM` 的 `RYNUI_DEPENDENCY_MODE`，分别通过校验后的 `FetchContent` archive 和 `find_package(SDL3 CONFIG REQUIRED COMPONENTS SDL3)` 产生 `SDL3::SDL3`，验证无效模式、内容校验失败或规范 target 缺失时 configure fail-fast
- [x] 2.2 添加 HLSL Quad shader 与离线 DXIL/SPIR-V 生成流程，支持 `RYNUI_SHADERCROSS_EXECUTABLE` host tool override；验证缺少工具链或交叉编译未提供 host tool 时 configure fail-fast，工具齐全时 build tree 生成对应 shader，应用目标不链接 `SDL3_shadercross` 运行时库
- [x] 2.3 实现 SDL init、Window、GPU device、claim 和逆序释放的 RAII 状态机，通过故障注入测试验证每个初始化失败点只释放已取得资源
- [x] 2.4 实现 UI 线程 command buffer、swapchain acquire、clear/present 与最小化空 texture 分支，在 Windows 真实窗口验证启动、至少一次帧提交、关闭和退出码
- [x] 2.5 运行本阶段自动测试与真实窗口 smoke，保存 GPU driver 和生命周期摘要，执行 `git diff --check` 后使用英文 commit message 提交本阶段

## 3. Fine-grained Reactive Runtime

- [x] 3.1 实现 `Signal<T>` 的读取收集、相等值抑制和精准通知，使用单元测试覆盖被依赖状态、无关状态和等价值写入
- [x] 3.2 实现缓存 `Memo<T>` 与动态依赖切换，使用单元测试证明有效缓存不重算、旧依赖解除、新依赖生效
- [x] 3.3 实现 `batch()`、去重 dirty queue 和 epoch 传播，使用单元测试证明多次写入只排队一次并产生最终稳定值
- [x] 3.4 实现 `Scope`、Effect 阶段、清理与重入保护，使用单元测试证明销毁后不通知、清理只执行一次、Effect 不发生无界同步递归
- [x] 3.5 添加 steady-state 更新分配计数 benchmark，证明已挂载图上的 Signal write/flush 路径不产生 heap allocation，执行全部 reactive 测试后使用英文 commit message 提交本阶段

## 4. 持久化 Node 与最小 Layout

- [x] 4.1 实现 generation-checked `NodeId`、稳定 slot 存储、parent/child 和销毁失效，通过单元测试证明旧 handle 不能访问复用 slot
- [x] 4.2 实现 Component 首次 mount 与 `Scope` 所有权，通过执行计数测试证明普通属性更新不重新执行 Component
- [x] 4.3 实现 `Constraints` 校验、内部 `BoxLayout` 和 horizontal/vertical `FlexLayout` 的 Measure/Place，通过边界和非法 Constraints 测试验证确定布局，且不暴露与后续 Ant Design `Flex`/`Space`/`Grid` 冲突的公开 API
- [x] 4.4 实现 `DirtyFlags` 到 Layout/Material/Transform/Geometry 队列的映射，通过计数测试证明 color/opacity/translation 不触发 Measure 或 Layout，size 会触发受影响布局根
- [x] 4.5 运行 Node、lifecycle、layout 和 dirty 全部测试以及 `git diff --check`，使用英文 commit message 提交本阶段

## 5. Quad Primitive 局部 GPU 更新闭环

- [ ] 5.1 实现 `QuadPrimitive`、CPU-side instance store、SDF Quad pipeline 和初次 GPU upload，通过离屏数据测试验证 instance layout 与 shader binding 一致
- [ ] 5.2 实现 Binding 到 Node property、Dirty queue、Primitive 和 instance range 的连接，通过集成测试证明 color/opacity/translation 只更新目标 Quad 数据
- [ ] 5.3 实现 frame request 与按需提交状态机，通过可控 clock/event 测试证明更新会请求帧、稳定后不按刷新率持续 submit、后续输入能重新唤醒
- [ ] 5.4 创建彩色 Quad 真实窗口示例，输出 Component、Signal、Observer、Measure、Layout、Primitive、GPU upload、submit 和 idle wake 计数
- [ ] 5.5 在 Windows 真实窗口触发 Material、Transform 和 size 更新，核对视觉结果与计数关系，执行全部测试和 `git diff --check` 后使用英文 commit message 提交本阶段

## 6. Linux 验收与 change 收口

- [ ] 6.1 在 Linux/Vulkan 环境完成 configure、build、CTest 和真实窗口运行，记录 GPU driver、shader format、退出码与计数摘要
- [ ] 6.2 在 Windows/D3D12 环境重新运行 clean configure、build、CTest 和真实窗口验收，确认 `BUNDLED|SYSTEM` 选择与文档一致
- [ ] 6.3 运行 `openspec doctor --json`、`openspec validate --all --strict --no-interactive` 和 `git diff --check`，更新任务与验收证据但不得提前勾选未通过的平台结果
- [ ] 6.4 确认 worktree 只包含本 change 相关文件，使用英文 commit message 提交最终验收阶段，并记录最终 commit SHA
