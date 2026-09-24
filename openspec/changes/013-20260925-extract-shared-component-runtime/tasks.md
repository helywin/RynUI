# Tasks

## 1. 窗口级服务所有权

- [x] 1.1 新增内部 `WindowComponentServices`，迁入 Text、interaction、hit-test、scene、surface、focus、pointer、animation 所有权；用构造/析构与唯一 owner 测试核对服务实例和依赖顺序。
- [x] 1.2 让 Button/Input 作为对等消费者使用共同挂载、销毁及帧同步入口；用混合声明顺序、identity、同步阶段、回调自毁和现有 Button/Input headless 测试验证无宿主间依赖。
- [x] 1.3 以 `windows-msvc` Debug 正式 preset 运行受影响 CTest、scene dirty/upload 和 idle benchmark，记录本机平台/编译器/preset；运行 `git diff --check`，以英文提交窗口服务阶段。

## 2. 窗口文本编辑设施

- [x] 2.1 将现有 editor store、唯一 text-input session 和 clipboard commands 置于窗口服务的按需编辑设施，Input 改用受限引用；以同窗重复绑定、冲突端口、销毁顺序和单一 session 测试验证。
- [x] 2.2 运行 Input editing、IME、selection、clipboard、controlled echo、caret 与 Button/Input 混合生命周期回归，检查 idle frame/无重复 session；运行 `git diff --check` 并以英文提交编辑服务阶段。

## 3. 按压行为

- [x] 3.1 提取内部 `PressableBehavior` 处理 primary pointer 捕获、inside release、cancel、disable、window blur、generation 复用和一次性 activation intent；以独立行为测试覆盖多指针、回调自毁与释放顺序。
- [x] 3.2 Button 改用 `PressableBehavior`，保留 Enter/Space、loading、hover、focus 与视觉策略；运行 Button pointer/keyboard/state、Input 混合回归、scene dirty/upload 和 steady-state allocation 测试，运行 `git diff --check` 并以英文提交按压阶段。

## 4. 集成验收

- [ ] 4.1 在一个受支持正式 preset 运行全部相关 unit/headless/contract/benchmark、public header、依赖锁与缓存检查，记录实际平台、编译器、preset 和结果；不在第二平台重复平台通用合同。
- [ ] 4.2 用 Windows MSVC x64 正式 preset 完成受影响 Debug/Release build 和平台集成 CTest，核对真实窗口的 Win32/D3D12/DXIL、输入、字体、退出码与已有 Button/Input 行为，单独保存 Windows 证据。
- [ ] 4.3 在 Linux 原生 Wayland 机器运行受影响 GCC/Clang 构建、平台测试及 Vulkan/SPIR-V、Fontconfig、输入实窗检查，单独保存 Linux 证据；不得以 Windows 或 XWayland 结果代替。
- [ ] 4.4 运行 `openspec doctor --json`、`openspec validate --all --strict --no-interactive`、`git diff --check`，核对 011 的共享阶段与 013 证据关系并更新其计划避免重复实现；以英文提交收口文档，不自动 archive 或 push。
