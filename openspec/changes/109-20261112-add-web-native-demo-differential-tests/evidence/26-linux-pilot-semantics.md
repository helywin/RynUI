# Linux 三例实际行为观测

- WSL Ubuntu 24.04 的 Linux GCC Release Gallery 经私有 Mutter Wayland 在隐藏 Vulkan/SPIR-V 上采集。设备为 llvmpipe Mesa 25.2.8，DPR 1。Button、Input、Select 各自的原生行为断言通过。
- 固定 Chromium 141.0.7390.37 无头进程运行三例 Web 动作，行为断言通过。Select 首次外层 240 秒预算不足，使用已有 180 秒 runner 参数复跑，外层总预算 300 秒，浏览器执行仍限 120 秒。完整 Select 运行有 10 个 Web 检查点。
- 本记录是实际输入与行为的双端证据。配对像素 profile 及 meta 红绿闭环仍待验证。
