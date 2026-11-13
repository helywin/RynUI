# Linear S1：Windows 独立验证

2026-11-14，Windows / MSVC / Ninja Multi-Config。只优化精确零项、24-bit 三因子的小乘积和片元零贡献早退；保留 raw-affine B 语义、1152-bit stop 谓词、ABI 与 admission。产品 paint/Linear/Conic/underlay/nonRound flags 继续关闭，53.1/53.2 未勾选。

- `windows-msvc-headless-debug/release`：8 个相关 portable targets 构建及 CTest 各 **8/8**；包含新增 2197 三元组、符号/carry/shift 与透明双场边界回归。
- `windows-msvc-debug/release`：相关 shader/embedded/native readback targets 构建；shader 合同/layout/完整 include closure 各 **8/8**，真实 hidden D3D12/DXIL transport 各 **1/1**。完整十项 shader 锁保留，paint closure 为 `4da725200bc6befdb1df7a8d50608d8133808b86eb7735b15fc4df4754ad41c9`。
- 同一 `--suite linear-cost`、55 秒 workload/60 秒外部 guard：Debug/Release 的 128×96 矩阵各 **15 cases / 1107895 checks**；1080p horizontal 单场与真实半透明双场各一帧、各 27 个 source 探针，四次均通过。成本期间无其他 C++ 构建或浏览器/GPU 测试。
- Release 1080p submit/fence/readback CPU wall：单场 **488.363 ms**、双场 **977.497 ms**；suite 总耗时分别约 1020/1488 ms。包含提交、等待和回读，不是 GPU kernel 时间或 FPS；driver cache 不受控。结果仍不足以支持日常 UI 吞吐，不能据此启用产品能力。

复用上一阶段的八个 portable target、shader 八项正则与 `^rynui\.effect_paint_d3d12_hidden$`；成本入口为同一 native readback exe 的 `--suite linear-cost --linear-size/--linear-case/--linear-layers`。clang-format 22.1.3 全仓检查、OpenSpec doctor/strict 与 diff 检查通过；原始 CSV、日志及图像只在 ignored `out/`。

未运行 Linux/Vulkan，也未完成 Conic/nonRound 全域认证、S2 或 Progress C。普通/cropped composition 共用 shader，但本轮没有新增 paint-specific crop/alpha 探针，不据此扩大 GPU 覆盖声明。
