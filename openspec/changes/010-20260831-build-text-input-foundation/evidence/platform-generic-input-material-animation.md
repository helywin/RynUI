# 平台通用：Input retained material 动画

2026-09-12；任务 7.2。接续 `platform-generic-input-state-materials.md` 的静态状态矩阵。

## 实现与边界

- Input 的背景、边框、文字、affix、caret、placeholder、selection 和八层 shadow color，以及 shadow opacity，通过共享 AnimationRuntime 的 owner scope/targets 过渡。
- 使用 Theme `motionDurationMid + motionEaseInOut`。相同目标不重启动画；快速反向从当前呈现值 retarget。shadow 几何使用目标 Token，颜色/透明度插值，不宣称几何 tween。
- reduced motion 和 Theme motion disabled 即时收敛。销毁 Input 释放 scope、targets、动画与 deadline；不新增轮询。
- 静态状态和 GPU range fixture 明确选择 reduced motion；正常动画测试单独驱动时钟，避免把瞬时目标值误当作正常动画的第一帧。
- 沿用固定 19 个 effect 槽位，Default hover 只改变既有 border，pointer/keyboard focus 均无额外 outline；状态切换不触发文字 shaping、measure 或 composer rebuild。

## 实际验证

Windows/MSVC，正式 `windows-msvc-debug` preset：

- Animation 与 helper 定向测试 15/15（1.10 秒）。
- Input component/GPU/material transition、Button component 与 motion policy 定向测试 5/5（3.83 秒）。
- 完整 CTest 197/197（282.50 秒）；其中 256 Input 普通 selection/composition-selection 各 20,000 次基准通过（143.99 秒）。
- 新增 helper 的 20,000 次全通道 retarget/tick 基准在预热后为 0 C++ heap allocation，最终无 deadline。
- 覆盖中间颜色、同目标 elision、快速 retarget、error/warning focus shadow、reduced motion、Theme motion disabled、销毁中断与 idle。

上述为平台通用/headless 与 CPU/fake GPU 合同，不是原生窗口、真实 GPU 视觉、Windows/Linux IME 或系统输入验收。
