# Windows Web 扩展回归

- Windows 锁定 CPython 3.14 与 headless Chromium 扩展回归执行 283 项，2 项失败、7 项按平台条件跳过。Pillow 测试首次未传入已锁定归档，使用 SHA256 校验的本地归档复跑后两项 Pillow 测试通过。
- Select 的错误选项顺序注入更新为实际编译产物中当前的赋值入口。锁定 Chromium 定向复跑通过。正常输入可执行，真实反转 options 后浏览器回报 `effective options mismatch` 并非零退出。本记录是 Web 侧故障检出，尚无配对视觉通过结论。
