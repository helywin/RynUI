# 隐藏 Input 的绘制实例身份

- Linux GCC Release 私有 Wayland 的 Select 样例在关闭虚拟列表后，一度把隐藏 Input 上次发布的 packed index 归给新实例，观测器拒绝输出。回归用真实 Input 和 RoundedEffectStore 复现序号复用，并验证当前可见边框仍绑定正确的 retained ID。
- Windows MSVC HEADLESS Debug、Release 的 `rynui.demo_differential_native_observation` 各 1/1 通过。Linux 修复后通过此前失败的检查点，生成 10/10 张回读；该轮达到 120 秒进程预算，完整行为流仍待复验。
