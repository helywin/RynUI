# 原生合同阶段证据

2026-10-02，Windows x64，MSVC / Ninja Multi-Config，任务 2.1 完成。

- `windows-msvc-headless-debug` Divider/Typography/交互/RoundedEffect math：4/4，1.69 s。
- `windows-msvc-headless-release` 相同：4/4，1.18 s。
- 补充 Theme algorithm/size 订阅、非法 reactive variant 与 4096 边界后，Divider 最终 Debug 1/1（0.66 s）、Release 1/1（0.48 s）。
- clang-format 22.1.3：398 tracked source files，0 failures；doctor healthy、full strict 45/45、diff check 通过。

既有 forms/reactive 测试全部保留。新增合同验证水平/垂直 Solid/Dashed/Dotted、legacy dashed 优先级、CPU 圆点 center/corner coverage 与透明 gap、无重测量/重排/slot 重跑、Material-only 圆点更新、translation/window clip、destroy 后 effects/ranges/interaction 清理；尺寸 override、Default/Dark/Compact map 派生、子 Theme inheritance/hash 与 metric subscription；全部五个标题方位及 RTL/物理位置、长度零/非零/过宽/窄容器，非法枚举/长度/token 的明确拒绝和 mount rollback。

整数索引分段先验证每组件 4096 上限；4096 可呈现，4097 以上明确失败且不部分替换已发布 scene，修正线宽后可恢复。Dotted 使用既有 logical RoundedEffect 零 blur 实心圆，无 backend 分支或 GPU ABI 改动。

源码说明：`docs/divider.md`。日志：`out/045-divider-tests.log`、`out/045-divider-boundary-tests.log`。整合/golden 与 Windows/Linux 原生验收分别属于后续任务，不将本阶段共同逻辑通过描述为真实窗口通过。
