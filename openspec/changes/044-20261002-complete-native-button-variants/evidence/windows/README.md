# Windows 原生验收

2026-10-02，Windows x64，MSVC / Ninja Multi-Config。

- `windows-msvc-debug` 完整 build 通过；受影响 CTest 69 项全部通过（首次 68/69，修复 demo 的旧拓扑断言后重跑失败项 1/1，0.10 s）。
- `windows-msvc-release` 完整 build 与受影响 CTest 69/69，42.20 s。
- 真实 SDL3 Windows 窗口：D3D12 / DXIL，Default/Dark/Compact 三主题；默认系统字体链；系统显示缩放实际为 1.25。
- Debug/Release × system/1/1.25/1.5/2，共 10 次运行，全部 exit 0；每次 25 张 GPU readback，总计 250 PNG。
- `runs.json` 记录参数、配置、EXE SHA256、PNG 尺寸和 SHA256；`validate_native.py` 验证运行前后 EXE 不变、全部图像与当前 EXE 的 hash。

每次窗口验证全部 16 color × 6 variant 原地切换，透明 ghost 与真实 dash gap，保留 custom icon/loading slot 和 start/end，icon-only Circle、Square、Round block，autoFocus/ref focus/blur、disabled focus 拒绝，loading 120 ms 延迟及 pending 可激活/active 不激活，内置 end spinner。真实 SDL 键盘和指针归一化触发 activation；wave 的 start/middle/finished 单独采样，进度递增且结束清理；resize 到 1420×900 后 block 宽度变化，失活后无下一帧 deadline，content/icon/loading slot 始终只执行一次。

人工查看了 2× 的 solid、ghost/dashed wave、dark 与 custom/built-in loading 图像；预设色、图标位置、不同形状、透明间隙和 wave 反馈正常。夹具隔离系统鼠标/焦点干扰，同时保留实际窗口 resize。未将该夹具称为系统 IME 验收。

Button demo 的共同绘制回归改为验证组件/node/interaction/scene/fragment 身份保持与 settled idle 无拓扑更新；有限 wave 增减 effect 时允许必要的拓扑更新。默认产品 wave 保持开启。

开发日志：`out/044-windows-build-tests.log`、`out/044-windows-repair-tests.log`、`out/044-windows-native.log`。Linux GCC/Clang、Vulkan/SPIR-V、Fontconfig/Wayland 原生任务保持独立 pending；本机结果不替代 Linux 证据。
