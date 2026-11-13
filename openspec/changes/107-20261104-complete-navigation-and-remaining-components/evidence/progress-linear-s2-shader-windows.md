# Linear S2 shader 基础阶段验收

- 平台：Windows / MSVC / Ninja Multi-Config，`windows-msvc-debug`、`windows-msvc-release`；实际 driver `direct3d12`、shader format `DXIL`，hidden readback。
- 实现：共用固定算术片段分别生成 12/72 个 base-2^16 digits 的 HLSL 路径；保留原 wide 回退、stop 选择与共同颜色合成。修正冻结候选中使用 HLSL 关键字 `point` 作参数名的编译阻断。
- 两配置分别构建 `rynui_shaders`、`rynui_embedded_shader_tests`、`rynui_effect_paint_native_readback_tests`。`generated_shaders`、`embedded_shaders`、glyph/quad/rounded-effect contract 及 effect-paint contract/layout/closure 共 **8/8**，Debug、Release 均无失败。
- `rynui.effect_paint_d3d12_hidden` 在两配置各 **1/1** 通过；Release 实际 transport 输出为 **63 cases / 36,503 checks**。该测试比较实际 GPU RGBA 与现有独立 source oracle，并保留上传/更新/idle、clip、混排及 source/ABI 合同；没有提高既有颜色容差。
- 完整十项 source closure 锁均保留。paint closure 为 `d8a689a06eaf68a0f220d2502eaf3f1f7c6f623a4aca107142f36a740da9a2b9`；paint-composition 为 `b5dd76f2732905b244ebee4b9d2d2564406c069b045d32fe7d2cec654af9a770`。
- 命令：`cmake --build --preset windows-msvc-debug --parallel 2 --target rynui_shaders rynui_embedded_shader_tests rynui_effect_paint_native_readback_tests`，随后运行上述八项 CTest 和 `^rynui\.effect_paint_d3d12_hidden$`；Release 使用对应 preset。
- 本阶段尚未合入新增 S2 位级 probe variants，未声明 actual composition crop-origin、S2 成本或 Linux/Vulkan 实跑通过。Debug/Release 回读耗时不作为 GPU kernel 或 UI 帧率证明。paint/Linear/Conic/underlay/nonRound 产品 flags 仍关闭，53.1/53.2 保持开放。
- 原始日志、生成 shader/reflection 和回读产物仅保存在已忽略的 `out/`。
