# Tasks

## 1. 参考与规划

- [x] 1.1 核对锁定的 Ant Design 6.6.5 Radio docs、group 和 style 来源，完成 proposal/spec/design 与公开状态矩阵；以 `openspec doctor --json`、strict validate、`git diff --check` 验证并英文提交规划阶段

## 2. Radio 与 Group 核心

- [x] 2.1 增加 `RadioProps`、`RadioGroupProps`、typed label slot 与公开聚合头，受控/默认模式互斥及重复 option 校验；以 public-only 编译和非法 Props 测试验证
- [x] 2.2 扩展 Selection 宿主的 Radio 状态、按压/Space/禁用/受控回写、Group 单值互斥及回调自毁；以 headless 指针、键盘、回写、销毁与 sibling tests 验证
- [x] 2.3 增加 Radio 的圆环/圆点、label、Group 布局与主题 Token 映射，保留 scene 身份及最小 dirty 范围；以几何、主题、CJK/Latin、scale 与 idle tests 验证
- [x] 2.4 在正式 `windows-msvc-debug` 构建并运行受影响 CTest、`git diff --check`，记录平台通用结果并英文提交实现阶段

## 3. Windows 专属验收

- [x] 3.1 将 Radio/Group 混排到 Gallery，在真实 Win32 窗口检查 1.0/1.25/1.5/2.0 acceptance scale、pointer/Space/Tab、禁用、标签、主题和退出；记录 D3D12/DXIL、系统字体、截图与诊断
- [x] 3.2 运行正式 `windows-msvc-release` 受影响 build/CTest、OpenSpec doctor/strict validate 与 `git diff --check`，保存 Windows 独立证据并英文提交，不主动 push

## 4. Linux 专属验收

- [x] 4.1 后续在实际 Linux 的 `linux-gcc`/`linux-clang` 正式 preset 构建和运行受影响 CTest，记录原生 Wayland/Vulkan/SPIR-V/Fontconfig 结果
- [x] 4.2 历史人工验收已关闭：后续在原生 Wayland 真实窗口检查至少两档实际 display scale、指针/键盘/主题/退出并保存独立截图和诊断；以英文提交 Linux 证据，不以 Windows 结果代替

## 5. Change 收口

- [ ] 5.1 仅在准备 archive 时核对平台通用、Windows、Linux 各项及证据，执行最终 strict validate 与工作树检查；不自动 push 或 archive

2026-11-02：本次 Linux 已验项的实际 preset、命令、结果与未验边界见 [Linux 验收摘要](../../../docs/validation/linux-native-20261102.md)。实际多输出缩放、系统 IME 与人工验收未由 render scale override 或内部注入代替。

2026-11-03：依用户明确指示，本文件标注“历史人工验收已关闭”的勾选表示取消人工任务门槛，非新增实测通过；混合项中的实现、构建、CTest 和平台自动集成仍独立追踪。后续人工体验建议仅写验收说明，不再新增任务，规则见 [人工检查与任务清单](../../../docs/development/openspec.md#人工检查与任务清单)。
