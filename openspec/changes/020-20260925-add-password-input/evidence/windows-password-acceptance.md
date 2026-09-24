# Password Windows 专属验收

- 环境：Windows 10.0.26200 x64，Visual Studio 2026 MSVC 14.51.36231；正式 `windows-msvc-debug` 与 `windows-msvc-release`、`Ninja Multi-Config`。Release 全目标构建通过，相关 CTest 11/11 通过；Debug 核心实现阶段完整 CTest 230/230 通过。Gallery 新增 Password 后，Debug 的 Gallery frame 与平台通用 journey 2/2 通过。
- 在真实 Win32 窗口运行 Gallery，诊断确认 D3D12、DXIL、系统字体 `Segoe UI Variable Text` 和 `Microsoft YaHei UI`。Default 主题 1.0、1.25、1.5、2.0 四档 acceptance scale 以及 Dark、Compact 的 1.5 档均通过。自动流程覆盖隐藏场景、窗口内编辑和组合输入事件、指针切换保留焦点与组合输入、Space 切回隐藏、禁用切换和正常退出；各次诊断的 `password_hidden`、`password_pointer`、`password_keyboard`、`password_disabled`、`exit_code` 均成功。
- 六张真实窗口截图已目视检查：Password 遮罩与切换文字可见、焦点边框和禁用颜色正确，Default/Dark/Compact 均无 Password 截断。截图及诊断保存在 `evidence/screenshots/windows-*.png` 和对应 `.txt`。
- Gallery 改动后，Search 与 Selection 的真实 Win32 1.5 倍 acceptance 均再次通过；OpenSpec doctor 健康、strict validate 20/20、`git diff --check` 通过。
- 初次 Gallery 样例中的 emoji 在 Windows 系统字体链的原文显示路径触发 `font_failure`，故真实窗口样例改为 CJK/Latin；复杂字素与 emoji 的遮罩及映射已由平台通用测试覆盖。窗口流程中的组合输入由应用内自动事件注入，不等同于人工操作的系统中文 IME 候选窗验收；该系统行为仍待独立验证。Linux 原生验收依用户安排暂缓。
