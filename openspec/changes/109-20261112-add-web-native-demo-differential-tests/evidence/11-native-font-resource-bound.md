# Native 字体资源边界修复

2026-11-14，Windows / MSVC / Ninja Multi-Config。实际五字体链运行检出：scenario 的通用 owned-file helper 将 JSON 的 16 MiB 限制也用于字体，拒绝锁定的 21,641,988-byte CJK 600 文件。

JSON 保持 16 MiB；显式测试字体另限每文件 32 MiB、最多 16 文件。checkout/out 归属、实际 SHA256、face/weight 与后续真实字体校验保持。错误信息区分 envelope 和字体资源，不改变普通产品字体接口。

`windows-msvc-headless-debug/release` 定向构建 `rynui_demo_differential_protocol_tests`，`^rynui\.demo_differential_native_protocol$` 各 **1/1**（内部各 6 项自动控制）通过：包含超过旧 JSON 大小的资源可解析、超限字体拒绝、JSON 上限保持及旧路径/source/grammar 负例。Release Gallery 重新构建通过。

实际 Gallery 已越过资源校验并完成 layout/upload，随后观察阶段报失效 ComponentId，仍按失败处理；本结果不是字体视觉或 paired pass。格式、OpenSpec doctor/strict 与 diff 检查通过，原始失败与复跑日志仅 out。
