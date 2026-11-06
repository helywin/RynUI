# Progress A / B1 平台通用合同

Windows/MSVC，正式 `windows-msvc-headless` Debug/Release、Ninja Multi-Config，最多 2 jobs。未启动窗口；原始构建与测试输出仅在 ignored `out/107-progress-ab1/`。

已实施 immutable premultiplied paint、Linear/Conic 与双场、显式 Butt/Square 的联合边界和 bounds、live unsupported preflight，以及独立 Effect Paint ABI 1 的 link/field/stop（16/32/32 bytes）打包和共同上传事务。保持 112-byte base 与旧 Quad/Glyph ABI；SDL 尚不宣称新 paint/cap，Recording 独立 opt in；旧 solid/default Round 零新增三表资源。

实际命令为 `cmake --build --preset windows-msvc-headless-debug` / `windows-msvc-headless-release`，构建对应 portable effect_paint、arc_cap、effect_preflight、effect_paint_packing、effect_paint_resources、rounded_effect_math/store/scene/gpu/gpu_resources、logical_scene_packing、scene_backend；CTest 同范围并含 backend_boundaries：两种配置均 **13/13** 通过。

回归实际覆盖精确旧 bytes、premultiplied 中点/双场 alpha、257 stops、厚帽与相交/近整圈、Square bounds、非零 first/混排/实际 Recording bytes、slot remap、逐表失败及后续 Image 失败、无新 CPU dirty 重试、owner/epoch/retire、idle 和 ColorPicker half-open solid field 同场。Recording 与 CPU reference 不能替代 GPU 数值证据。

clang-format 22.1.3 全仓及新增源检查通过；doctor healthy、strict 108/108、diff 检查通过。53.1/53.2 保持未完成：B2 HLSL/SDL、原生 readback、组件 C 仍待实施验证。B2 已另发现大半径细描边的 FP32 边界坍缩反例，正在 Astra 复核；本阶段 double CPU 几何通过不能背书该 GPU 路径。Linux/Vulkan、系统输入或 GPU 像素均无本阶段通过结论。
