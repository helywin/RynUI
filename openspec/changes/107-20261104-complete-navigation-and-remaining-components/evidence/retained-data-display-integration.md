# 保留式数据展示组件主线集成验收

2026-11-05，main 基线 `acf7e7f3774ab9d5aeb630c38d6abc171f74729c`。按顺序语义合入 `7a78fc7d582be63563723f7be53de9912bc55d65`、`193f8feaeaa9a92951d6f8c833276992926a5145`、`b6f919fa0cdb85fd2983aea8dd9ddb746fa88eb2`、`ac887e72e325e6be7a1164aa7078b16e174bc4d1`、`2923be50839196224cf2feaf30c2a38d78587bb9`。源 [Collapse](collapse-linux.md)、[Descriptions](descriptions-linux.md)、[Segmented](segmented-linux.md)、[Timeline](timeline-linux.md) 是旧隔离基线证据，不替代下列主线结果；两个源工作树均保持只读、干净、无 upstream，未推送 T3 分支。

保留四组件完整 typed API、局部 keyed slots/revisions、实际草稿/ref、Theme/语义字体与前景色、显隐/输入/子浮层清理及有限 motion。新 DataDisplay helper 按 owner/generation 追加与退役区域。共同 opacity 沿用 Stage A，不新增全局 alpha 或上传路径。新增 Carousel→lazy Collapse→Descriptions→Image/Form/Popover/Select 组合覆盖非 1 因子下创建/替换、零/恢复、同值 reset/preedit、Enter、内层 Escape、面板返回与关闭后的 idle/退休。

合并回归复现 Segmented 删除选中项时使用保留行旧 disabled 值回退的问题：将回退移至整组新 bindings 发布后，保留“仅禁用仍存在的选中项不改变其状态”的规则。组合脚本初次点击 Popover 容器空白，改为实际 owner Button 的几何后通过，未修改 Popup 实现或放宽断言。首次完整 Debug 组合还发现 source 合同仍固定旧 93 样例；按 writer 的实际 97 修正，并新增四个 Calendar/Carousel 样例 typed slots/tokens 元数据断言。

实际平台 Linux/GNU，Ninja Multi-Config，构建均 `-j2`，仅 36 个受影响 target。HEADLESS Debug 首轮 **55/56**，唯一 source 合同失败修复后定向 **1/1**，56 个不同检查全部通过；Release **56/56，0 失败**。包含四组件核心 37、四页 Gallery 4、目录/Theme/Core/public API/共同 opacity/Form/组合及文档检查 15。10 个旧/新公开文档例子由实际 C++ 编译器解析，两配置检查均通过；未运行全量 CTest。

```text
cmake --preset linux-gcc-headless
for configuration in debug release; do
  cmake --build --preset "linux-gcc-headless-${configuration}" --target \
    rynui_collapse_{public_api,theme,component,contract}_tests \
    rynui_segmented_{public_api,theme,component,contract}_tests \
    rynui_descriptions_{model,public_api,theme,component,integration,region_clip,shared_theme,contract}_tests \
    rynui_timeline_{model,public_api,theme,component,integration,region_clip,example,review,contract}_tests \
    rynui_{data_display_gallery,theme_algorithm,gallery_sample_page,gallery_source_panel,ant_design_reference_catalog,retained_data_display_integration,paint_opacity,paint_opacity_integration,form_integration}_tests \
    rynui_portable_{theme_public_api,logical_scene_packing} -j2
  ctest --preset "linux-gcc-headless-${configuration}" --output-on-failure -R '^rynui\.(collapse_|segmented_|descriptions_|timeline_|theme_algorithm$|portable\.(backend_boundaries|theme_public_api|logical_scene_packing)$|ant_design_reference_catalog$|gallery_sample_page$|gallery_source_panel$|gallery_sample_source_contract$|gallery_sample_source_generated$|data_display_docs_examples$|paint_opacity($|_integration$)|form_integration$|retained_data_display_integration$|calendar_carousel_theme_baseline$)'
done
```

Linux `linux-gcc` Release Gallery/opacity GPU target 实际构建。仅经现有 runner→独立 headless Wayland/private DBus，实际 Vulkan/SPIR-V、系统字体、host input area/cursor：**5/5 cases、285 checks、58 submitted frames、58 次实际 frame callback，0 失败**。有限 Collapse→Segmented→Descriptions→Timeline→Button 序列覆盖源真实动作，并在下一次 resize 前主动导航退役，检查 root/capture/session/editor generation。保留 106 callback、动作 pacing、正常 SDL pump 和 reveal 后启用示例 motion 的顺序；未重复 Stage A 完整 Gallery 序列。

```text
python tools/run_gallery_page_acceptance.py out/build/linux-gcc/examples/Release/rynui_token_gallery --log-dir out/retained-data-display-integration/native --preset linux-gcc --configuration Release --timeout 120 --page collapse --page segmented --page descriptions --page timeline --page button
python tools/run_background_wayland.py --log-dir out/retained-data-display-integration/gpu --timeout 60 -- out/build/linux-gcc/tests/Release/rynui_paint_opacity_gpu_tests out/retained-data-display-integration/gpu/readback
```

实际 Vulkan 组合 fixture 新增 **32 checks**（四幅读回，含资源/attachment 与 RGB 断言）：lazy 展开时半透明 Image、零/恢复、局部替换后的蓝色半透明 Image，以及真实 Form 自动绑定/状态保留。该同一 fixture 总 **2211 checks**，其中旧 Stage A 的 2179 不重复计为新增；RGB 容限保持 10/255，raw readback 仅在 out。

catalog/source writer/check/self-test 通过：**97 samples 的全部 ID 与顺序不变，458 frozen legacy IDs 的 inventory 逐字节不变**；只修改四条目标 overlay。实际 Theme writer 重生成后，**5 份完整 golden 与 Stage A 逐字节一致，各 66 个顶层字段**，无 domain/TokenIdentity/聚合顺序变化。clang-format 22.1.3 检查 **1022** 个自有源，0 失败；doctor healthy、strict **107/107**、diff check 通过。logical CPU scene v3、四类既有 packed ABI、Core 无 renderer/SDL/平台反向依赖保持。

本阶段仅新增关闭 31.1/31.2、32.1/32.2、39.1/39.2、43.1/43.2，总进度 **68/135**；既有完成项保持，63.3、64.*、65.1 仍开放。未合入 Table 未完成代码、ownership 字段或其有限 case，未改 Modal/Tabs/Gallery/Table translation；后续 ownership 须与既有 Node paint opacity 共存。Windows/MSVC/D3D12、物理 IME/AT、真实桌面与跨物理显示器未验证；无人工 checkbox、桌面 fallback、物理输入或 proxy 改动。原始日志/JSON/图像/二进制只在忽略的 out。
