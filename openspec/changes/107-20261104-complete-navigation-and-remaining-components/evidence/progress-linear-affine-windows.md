# Progress raw-affine Linear 候选：Windows 验证

2026-11-14，Windows / MSVC / Ninja Multi-Config。Paint ABI v2 保留 raw domain/affine，field48；Linear 采用已选定的 B 语义和 1152-bit stop 判定。基础 scene/stride 与 cap32 保持；所有 SDL 产品 paint/Linear/Conic/underlay/nonRound flags 仍关闭，53.1/53.2 未勾选。

- `windows-msvc-headless-debug/release`：8 个 portable targets 构建、定向 CTest 各 **8/8**。范围为 effect_paint/packing/resources/preflight/compose_guard、rounded_effect_gpu/gpu_resources、cap_geometry；独立动态 base-2^32 source oracle 与资源事务通过。
- `windows-msvc-debug/release`：shader、embedded 与 native readback targets 构建通过。生成/嵌入、Glyph/Quad/Effect 合同、paint 合同/layout/closure 各配置 **8/8**；实际 hidden D3D12/DXIL transport 各 **1/1**，每配置 **63 cases / 36503 checks**，idle upload delta 为零。初跑发现测试仍按 field32 计算越界地址，改用中央 field stride 后复跑通过；另修正新 oracle 的 Color 命名空间编译错误。
- 原生 `--suite linear-cost` 保留 55 秒 workload/60 秒外部 guard。128×96 的六种输入、单/半透明双场及 idle/paint/metrics 更新共 **15 cases**，Debug/Release 均通过；逐像素使用独立 source oracle。1920×1080 单场与两个半透明 Linear 各一帧、各 27 个 source 探针，两配置均通过。
- Release 1080p 的 submit/fence/readback CPU wall time：单场约 **1845 ms**，双场约 **3315 ms**。它含提交、等待和回读，不是独立 GPU kernel 时间或 FPS；driver cache 状态不受控。通过 guard 不代表 UI 吞吐可用，该结果用于下一阶段最小优化。

CPU 复用：`cmake --build --preset windows-msvc-headless-{debug,release} --target rynui_portable_effect_paint_packing rynui_portable_effect_paint_resources rynui_portable_effect_preflight rynui_portable_effect_paint rynui_portable_rounded_effect_gpu rynui_portable_rounded_effect_gpu_resources rynui_portable_cap_geometry rynui_portable_effect_compose_guard --parallel 2`。native 复用 `rynui_shaders/rynui_embedded_shader_tests/rynui_effect_paint_native_readback_tests` 与 `^rynui\.effect_paint_d3d12_hidden$`；成本入口使用相同 exe 的 `--suite linear-cost --linear-size/--linear-case/--linear-layers`。

格式、OpenSpec 与 diff 按阶段校验；原始 CSV/RGBA/logs 只在 ignored `out/`。未完成 Linux/Vulkan、Conic 全域谓词、非 Round 数值/预算认证或 Progress C 消费，不把普通 compatibility 样本描述为全域证明。旧 logical float/double reference 仍是近似检查，不替代源公式期望。
