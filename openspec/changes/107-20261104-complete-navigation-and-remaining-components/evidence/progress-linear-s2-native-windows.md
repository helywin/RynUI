# Linear S2 位级探针与真实 composition crop 验收

- 实际平台：Windows / MSVC / Ninja Multi-Config，`windows-msvc-debug`、`windows-msvc-release`，隐藏 D3D12/DXIL 回读；窗口不显示、不抢焦点。
- 接入 13 个固定 test-only fragment variants：正常 auto/wide、选择与 fraction 位级探针、路径判据和字节 canary。生成器核验双 storage buffer 与完整七项输入签名；修复 shadercross 裁掉输入后造成的 D3D12 VS/PS register linkage 错误。
- Debug、Release 各 **12/12 CTest** 通过：九项 shader/embedding/layout/closure，以及 composition、Linear S2、旧 paint transport 三项真实 GPU 回读。S2 每配置均为 **396 cases / 24,318 checks**；旧 transport 每配置为 **63 cases / 36,503 checks**。
- S2 包含六个实际较小 composition target 的 Begin/DrawRange/End 场景，验证非零 crop `(10,5,19,17)`、first/stop 偏移、48 个独立 GLOBAL source oracle 点及 2,988 个严格零色 guard 点。正常 composition shader 没有 forced-wide 测试入口，不声明该对照已完成。
- scene scale 1/1.5/2 是模拟 SceneDeviceMetrics；实际平台 scale 为 1.5。没有实际跨显示器或多系统 DPI 验收，也没有用 SPIR-V 编译/反射代替 Linux/Vulkan 实跑。
- Release 同条件有界成本入口三次均退出 0：128×96 全矩阵（15 cases）及 1920×1080 horizontal 单/双 Linear。保留原 55 秒 workload / 60 秒外部 guard、独立 source oracle 和容差；记录的是 CPU wall clock，含初始化、提交/fence/readback、oracle/I/O，driver cache 未受控，不作为 GPU kernel 或 UI 帧率证明。
- 构建相关 targets：`rynui_linear_s2_native_tests`、`rynui_effect_paint_native_readback_tests`、`rynui_composition_native_readback_tests`、`rynui_embedded_shader_tests`。CTest 使用 `^rynui\.(generated_shaders|embedded_shaders|glyph_shader_contract|quad_shader_contract|rounded_effect_shader_contract|effect_paint_shader_(contract|layout|closure)|composition_shader_layout|composition_d3d12_hidden|effect_paint_d3d12_hidden|linear_s2_d3d12_hidden)$`，两配置均通过。
- clang-format 22.1.3、OpenSpec doctor/strict 与 diff 检查通过。原始失败、D3D12 验证层输出、生成 shader/reflection、RGBA 和成本记录仅保存在 ignored `out/`。
- paint/Linear/Conic/underlay/nonRound 产品 flags 继续关闭；Conic/nonRound 数学认证及本阶段 Linux GPU 验证未完成，53.1/53.2 保持开放。
