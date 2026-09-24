# 平台通用实现回归

- 环境：Windows 10.0.26200、Visual Studio 2026 MSVC 14.51.36231，正式 `windows-msvc` Ninja Multi-Config preset 的 Debug 配置。
- `pwsh -NoProfile -File scripts/build-windows.ps1 -Configuration Debug -SkipTests` 构建通过。
- 定向 CTest 12/12 通过：Input clear、Input 公共 API/组件/指针/键盘、Password、Search、Gallery frame/平台通用 journey 和焦点序列。
- 新增测试实际覆盖响应式显示/折叠、自定义 suffix 保留、指针与键盘激活、IME 组合输入取消、受控回写、禁用/只读、焦点与 session、清空回调自毁后的资源释放。
- `git diff --check` 通过。真实 Win32 窗口与 Release 构建另行记录。
