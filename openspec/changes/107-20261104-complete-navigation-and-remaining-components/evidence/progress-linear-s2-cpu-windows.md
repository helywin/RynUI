# Linear S2 CPU 阶段验收

- 平台：Windows，MSVC，Ninja Multi-Config；`windows-msvc-headless-debug`、`windows-msvc-headless-release`。
- 范围：固定 12×16-bit 窄核资格判断、原 72×16-bit 精确回退和共同 Linear selection。raw-affine 语义、field48 ABI、admission、Conic、颜色合成和产品能力保持现有合同。
- 两个配置均构建下列八个 portable target，并以同名 CTest 执行：`effect_paint_packing`、`effect_paint_resources`、`effect_preflight`、`effect_paint`、`rounded_effect_gpu`、`rounded_effect_gpu_resources`、`cap_geometry`、`effect_compose_guard`（target 前缀 `rynui_portable_`，CTest 前缀 `rynui.portable.`）。Debug **8/8**、Release **8/8**，无失败。
- `effect_paint_packing` 新增矩阵实际执行了整数中间量、stop 选择、fraction bits、auto/forced-wide、192-bit 资格边界与回退原因检查；独立 source oracle 未修改。范围包括 k90–93、scale=2、联合 pair 失败、277-bit stop 跨度、重复点、极端抵消及 513 stops。
- 构建命令：`cmake --build --preset windows-msvc-headless-debug --parallel 2 --target <上述八项>`；Release 使用对应 preset。CTest 正则：`^rynui\.portable\.(effect_paint_packing|effect_paint_resources|effect_preflight|effect_paint|rounded_effect_gpu|rounded_effect_gpu_resources|cap_geometry|effect_compose_guard)$`。
- 本阶段未合入 S2 HLSL/native 测试增量，未声明 S2 shader、D3D12/Vulkan、composition crop-origin 或性能通过。此前 S1 GPU 结果不替代本阶段后续 GPU 验收；paint/Linear/Conic/underlay/nonRound 产品 flags 继续关闭，53.1/53.2 保持开放。
- 原始构建与 CTest 日志仅保存在已忽略的 `out/`。
