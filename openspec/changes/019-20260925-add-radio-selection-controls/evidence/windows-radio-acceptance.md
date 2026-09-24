# Radio Windows 专属验收

- 机器：Windows 10.0.26200 x64；Visual Studio 2026 MSVC 14.51.36231；正式 `windows-msvc-debug`/`windows-msvc-release` preset，CMake `Ninja Multi-Config`。Release 全目标构建通过，受影响 CTest 8/8 通过；新增主题清屏色后，Debug 与 Release 各自的 Radio、Selection、Search、Input Journey、Gallery frame 定向 CTest 均为 6/6 通过。
- 真实 Win32 Gallery 使用 D3D12、DXIL、系统字体 `Segoe UI Variable Text` 与 `Microsoft YaHei UI`。Default 主题在 acceptance scale 1.0、1.25、1.5、2.0 均执行自动 Tab、Enter、Space、指针选择、重复选择与 disabled 拦截，`selection_keyboard=true`、`selection_pointer=true`、`selection_blocked=true`、`automated_input_events=31`、`exit_code=0`。四张截图已目视检查 Radio/Group 的圆环圆点、CJK/Latin 标签、互斥与禁用外观；诊断和截图保存在 `evidence/screenshots/windows-default-scale-*`。
- Dark 与 Compact 分别在真实窗口的 1.5 acceptance scale 完成同一输入流程与退出检查，截图和诊断保存在 `evidence/screenshots/windows-dark-scale-1.5.*`、`windows-compact-scale-1.5.*`。Dark 首轮截图暴露 Gallery 固定白色清屏色，造成浅色文字不可见；已让 Gallery 从主题容器色设置 SceneRenderer 清屏色，并重新运行。最终 Dark 截图显示深色背景上可辨认的 Radio 标签、圆环和其它控件。Compact 截图显示压缩布局无截断。
- Gallery 变更后，Search 的真实 Win32 acceptance 在 1.5 倍通过，`search_submits=3`、`exit_code=0`；Button/Input/Switch/Checkbox 与 Radio 在同一窗口继续共用服务。
- `openspec doctor --json` 健康，`openspec validate --all --strict --no-interactive` 19/19 通过，`git diff --check` 通过。
- 这是 Windows 系统输入、D3D12 与系统字体证据；Linux Wayland/Vulkan 不由这些结果代替。014 原生中文 IME 候选/合成仍待独立人工验收。
