# Tasks

## 1. 参考与规划

- [x] 1.1 核对锁定 Ant Design 6.6.5 Password 行为、现有 Input/IME/剪贴板链路，完成 proposal/spec/design；运行 OpenSpec doctor、strict validate、`git diff --check` 并英文提交规划阶段

## 2. 展示投影与 Password 核心

- [ ] 2.1 为 Input 展示层实现每字素一个遮罩符号、composition/selection/caret 双向映射与异常安全；以 UTF-8、复杂字素、预编辑、滚动和非密码回归验证
- [ ] 2.2 增加 Password typed Props、可见性受控/默认模式、Input 复用、焦点保持切换策略与隐藏剪贴板门禁；以 headless 指针、键盘、回写、禁用和生命周期测试验证
- [ ] 2.3 增加平台密码输入类型及 SDL 映射，验证组合输入期间切换不会取消当前输入；以会话端口测试验证
- [ ] 2.4 正式 `windows-msvc-debug` 构建并运行受影响 CTest、`git diff --check`，记录平台通用结果并英文提交实现阶段

## 3. Windows 专属验收

- [ ] 3.1 Gallery 混排 Password/Input，在真实 Win32 窗口检查 1.0/1.25/1.5/2.0 scale、焦点、指针切换、编辑、禁用、主题和退出；记录系统字体、D3D12/DXIL 截图与诊断
- [ ] 3.2 运行正式 `windows-msvc-release` 受影响构建/CTest、OpenSpec doctor/strict validate 和 `git diff --check`，保存 Windows 独立证据并英文提交

## 4. Linux 专属验收

- [ ] 4.1 后续在真实 Linux `linux-gcc`/`linux-clang` 正式 preset 构建及运行受影响 CTest，记录 Wayland/Vulkan/SPIR-V/Fontconfig 结果
- [ ] 4.2 后续在原生 Wayland 窗口检查实际 display scale、指针、键盘、系统输入、主题与退出，独立保存截图和诊断并英文提交

## 5. Change 收口

- [ ] 5.1 仅在准备 archive 时核对平台通用、Windows、Linux 各任务和证据，完成最终校验；不自动 push 或 archive
