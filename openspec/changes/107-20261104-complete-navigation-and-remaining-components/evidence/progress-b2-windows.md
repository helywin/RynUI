# Progress B2：Windows paint transport 与投影阶段

2026-11-07，基于 `0742f916` 后的本阶段源码。**B2 尚未整体完成，53.1/53.2 保持未勾选。** SDL 产品 paint/Linear/Conic/underlay/nonRound 能力全部保持关闭；GPU 验证仅使用当前 backend owner 的测试作用域。

## 实现范围

- Round paint 使用独立 link/field/stop，保留 Effect112 及旧 solid 冷路径；新增 raw32 cap 的 CPU pack、保守 bounds 和共同资源事务，未生成或启用 cap shader。
- 宿主显式传递 display scale，compose/upload 使用同一 AA guard；无文字场景覆盖 `.25 → 1 → .25`、原 draw 的 ID/first/count、几何更新和 idle。
- 新旧 effect 共用 coverage include；锁文件覆盖完整 source closure、stage 编译定义与依赖。fragment 采样/storage 不进入 vertex 模块，保留真实 SPIR-V ABI/Flat/offset 检查。
- 修复隐藏双窗口验证暴露的 SDL 生命周期问题：每个 PlatformState 配对获取/释放 video 子系统引用，销毁另一个窗口后原窗口仍有效。全局 SDL 生命周期不由单个窗口释放；依据 [SDL3 初始化引用计数](https://wiki.libsdl.org/SDL3/SDL_Init)与 [SDL_QuitSubSystem](https://wiki.libsdl.org/SDL3/SDL_QuitSubSystem)。

## 实际自动结果

| 环境 | 命令范围 | 结果 |
| --- | --- | --- |
| Windows / MSVC / `windows-msvc-headless-debug` | paint、packing、resources、preflight、arc_cap、rounded_effect math/store/gpu/resources、cap_geometry、effect_compose_guard 共 11 个相关 target/CTest | 11 个不同测试通过；初次 guard 夹具缺默认字体链，补齐后重跑通过 |
| Windows / MSVC / `windows-msvc-headless-release` | 同一组 11 个 target/CTest | 11/11 通过 |
| Windows / MSVC / `windows-msvc-debug` | `rynui_shaders`、`rynui_embedded_shader_tests`、`rynui_effect_paint_native_readback_tests`；generated/embedded、Quad/Glyph/Effect、paint contract/layout/closure | 8/8 通过，实际生成 DXIL/SPIR-V；SPIR-V 反射不等于 DXIL 反射 |
| Windows / D3D12 / DXIL / hidden / Debug | `rynui.effect_paint_d3d12_hidden` 正式 CTest（exe 使用 `--suite transport`）；owned RGBA readback | 1/1 通过，56 个计数用例、36,391 项检查，另含 owned upload 独立读回，约 1.8 秒 workload |
| Windows / MSVC / native Debug | `rynui.platform_lifecycle`，并由上述真实双窗口读回覆盖窗口存活 | 1/1 通过 |

构建均使用正式 CMake presets、Ninja Multi-Config、最多 2 compiler jobs。CPU 精确选择/插值、实际两流/两 storage、非零 first、solid/paint 混排、双场 alpha、分数 DPI/hard clip、owned bytes、上传失败重试和 idle 均在上述定向范围内；未重复运行无关全量 suite。原生测试校验实际 driver=`direct3d12`、format=`DXIL`，没有打开可见窗口。

原生阶段修正了 HLSL 保留字/结构体三元表达式、vertex 未用 storage 声明，并将跨整圈用例改为实际 canonical `-pi_f` 的可达输入；预期仍来自独立角度不等式，未放宽颜色容差。生成物、原始 log、逐捕获 RGBA 和运行索引只在 ignored `out/`。

## 未完成边界

Conic 全域误差/跨 stop 判定与 nonRound 联合外边界认证仍未完成；有限矩阵通过不解决这些证明。cap native CTest 保持 DISABLED，不计通过；没有 Progress C 消费者验收，也没有真实 Linux/Wayland/Vulkan、系统 IME、AT 或人工桌面证据。本阶段不提升组件目录支持状态。
