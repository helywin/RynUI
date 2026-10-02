# 平台通用实施与验证

2026-10-02，实际环境 Windows/MSVC 14.51.36231、Ninja Multi-Config，configure 为 `windows-msvc-headless`，使用对应 Debug/Release build/test presets。

- 完成 R8 source view、源/目标 validator、atlas 直接借用、SDL adapter mapped packing、Recording 紧凑 owned pixels 与 consumers 迁移；共同层无 backend transfer fields/alignment capability/逐区域 staging。
- 完成必需能力/版本与输入上限 manifest，unsupported 在创建 GPU 资源前拒绝，超限在 begin 前拒绝；Quad preflight 与实际增长共用 helper，避免非二次幂旧容量翻倍时低估申请。Effect 用 live count 保守估算，不修改 Core cull 结果。
- Debug 完整 CTest 35/35（117.65 秒）；新增 Quad 增长上限断言后相关 5 项 5/5（1.55 秒）。Release 完整 CTest 35/35（11.38 秒），包含最终断言。
- literal 源 offset/stride/短末行、R8 pixels/未覆盖目标、零 padding、256/512 alignment、chunk/oversize/uint32 overflow、caller lifetime、invalid 不消耗注入、cancel/commit retry/epoch 重建、11 项能力负例、三类 buffer/atlas 上限与 dirty/附件恢复、空 atlas、恰好上限均通过。
- 原有真实组件 scene、局部/idle 更新、CPU atlas 逐字节比较、packed ABI 与依赖边界合同通过。Release 256 个 Input 分别 20,000 次 selection/composition-selection 更新，allocations、dispatch_allocations、sync_allocations 均为 0（2117/2391 ms）；Effect allocation 合同通过。耗时为本机 CPU 记录，不是 GPU 时间或速度提升结论。
- doctor healthy，全量 strict 38 passed、0 failed，working/staged diff check 通过。

CTest 原始结果见 `common-debug-ctest.txt`、`common-debug-final-checks.txt`、`common-release-ctest.txt`、`common-release-details.txt`。这是平台通用合同的一次实际验证；Windows GPU/窗口单独留证，Linux 原生项仍未完成。
