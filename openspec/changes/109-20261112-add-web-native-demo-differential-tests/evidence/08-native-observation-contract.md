# Native semantic observation 与回读合同

2026-11-14，Windows / MSVC，正式 Ninja Multi-Config presets。

- `windows-msvc-headless-debug`、`windows-msvc-headless-release` 均构建 inventory、native protocol/input/observation 和四个 Gallery finite/host targets。定向 CTest 各 **9/9**，分别 477.46 秒、85.06 秒；包含旧 sample/page/controller/host 回归。
- CTest 范围：`^rynui\.(demo_differential_(inventory|native_(protocol|input|observation))|gallery_page_acceptance_(options|runner)|gallery_sample_page|gallery_page_controller|gallery_pages_host)$`。
- 新观测测试使用真实 retained Gallery、CPU capture port 和 Recording 合同；覆盖实际文字/icon/字体来源、owner-local 代际与退役、合法 shell deadline、旧 editor session、旧 scene/fence、错误 DPR/channel/source/binary 和未重建来源。它不栅格化或证明 GPU 完成。
- SDL 回读路径新增由真实 submit/wait/map 产生的 owner-local receipt；场景 attachment 固定提交 metrics/revision。checkpoint receipt、surface submission counter 与 compositor/presented 事实分开，未知呈现不填默认 false。
- 显式 pinned 字体不补系统/mono/bundled fallback，缺请求字重/斜体拒绝；普通 Gallery 系统字体默认保持。600 字重、emoji 覆盖与完整 paired 字体准备仍是后续范围。
- `windows-msvc-release` 构建 Gallery 通过；`rynui_default_font_chain_tests` 正式 native Debug/Release 构建与 `^rynui\.default_font_chain$` 均 **1/1**，分别 2.88 秒、2.26 秒。该项实际验证 Windows 字体加载及显式链拒绝策略，不是 Web 字体匹配或 GPU 图像结论。
- Release 实际 D3D12/DXIL：`effect_paint_shader_closure/composition_shader_layout/composition_d3d12_hidden` **3/3**。初跑由新加的 scene/window DPI 相等检查阻断既有模拟 DPI 矩阵；恢复原回读准入并断言 receipt 分别保留 scene/target metrics 后复跑通过。真实 hidden RGBA 覆盖多比例合成，未观察 compositor 呈现。
- clang-format 22.1.3：1228 tracked sources 与 3 个新增 C++ 文件检查通过；OpenSpec 2.0.0 doctor healthy、strict **109/109**，`git diff --check` 通过。

据上述 CORE 及实际字体合同完成 3.4–3.6。本结果不代表 Web/native paired、平台校准、真实 fault 红绿、完整差分 GPU 矩阵、被动 idle 或完整工厂输入等价通过。原始日志与运行产物只在 ignored `out/`。
