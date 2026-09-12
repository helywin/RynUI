# 平台通用：Input caret deadline 与 idle

2026-09-12；任务 7.5；Windows/MSVC `windows-msvc-debug`。

## 实现

- 每个 retained Input 持有离散 caret 状态，500,000 微秒翻转一次，不依赖显示刷新率；错过多个 deadline 时按当前 phase 收敛，不逐次补帧。
- 合法 text input（包括无 value 变化的输入）、composition update、selection/navigation 和 pointer down 重置显示及下一 deadline。
- focus/window loss、disabled/read-only 清除 deadline；恢复编辑和 normal motion 后重新调度。Theme motion disabled 或 reduced motion 保持静态 caret；owner 销毁后不会留下 deadline。
- ButtonComponentHost 聚合自身 AnimationRuntime 和 auxiliary component 的最早 deadline，统一 tick。Button/Layout/Token Gallery 的真实运行循环和对应 headless frame fixture 都改用该聚合源，避免只等待材料动画而漏掉 Input caret。
- deadline 在已调度帧内只标记 Material dirty，不请求冗余帧；retained Quad opacity 更新不改变 topology、glyph、atlas 或 shadow。
- 极大 clock 值无法继续调度时保持静态/idle；重复或倒退采样不重复翻转。

## 验证

Input component/pointer/keyboard/caret/GPU、Button/Layout/Token Gallery frame 和 frame scheduler 定向 9/9 通过（17.83 秒）。最终增加无 value 变化 TextCommitted 重置测试，caret 测试再次通过（0.06 秒）。

受控时钟测试覆盖 60/120/144 Hz 两秒内精确四次变化、missed deadline phase、input reset、motion policy、clock exhaustion；20,000 次 deadline tick 为 0 C++ heap allocation。

真实 OnDemandFrameLoop + Input host + CountingGpu 合同验证：首次直接等待剩余 300 ms，后续等待 500 ms；每次闪烁仅一次 Quad upload，无 glyph/texture/effect upload，且无冗余 pending frame。reduced motion 后连续四次 frame-loop step 都是 idle、submit 计数不再增加。另验证 Theme motion=false、read-only、window loss、disabled、destroy 后 deadline 清除与 shape/measure/composer 不增长。

最终完整 CTest 200/200 通过（294.18 秒），其中 256 Input 普通 selection/composition-selection 各 20,000 次基准通过（168.33 秒），其零分配、固定容量及最小 dirty/GPU range 断言全部满足。OpenSpec doctor、全部 10 项 strict validation 与 git diff --check 通过，阶段 7.6 收口。

上述为 headless/CPU/fake GPU 证据，不代替真实窗口、驱动、系统 IME 或人工视觉验收。
