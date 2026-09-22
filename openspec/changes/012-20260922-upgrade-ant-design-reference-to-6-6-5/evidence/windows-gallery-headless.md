# 6.6.5 Gallery / 当前文档平台通用验证（Windows 执行）

- 环境：Windows 10.0.26200；`windows-msvc` / Ninja Multi-Config / Debug / MSVC x64 19.51.36256，仓库 `scripts/build-windows.ps1 -Configuration Debug -SkipTests` 构建通过。
- 当前离线参考目录：七类 73 项；Data Display 从 20 增至 21，List 保留并标记 deprecated，Listy 新增为 planned。Foundation/Token 文档使用 1198 条 6.6.5 catalog；14 个 live sample 仍是已存在的真实 RynUI 组件，不为目录条目造交互样例。
- `ctest --preset windows-msvc-debug -R '^(rynui\.(ant_design_(reference_catalog|gallery_catalog_generator|gallery_catalog_contract|current_baseline)|design_token_catalog|gallery_document_|token_gallery_(frame|benchmark)|reference_surface))' --output-on-failure`：14/14 PASS（含目录、过滤、锚点、wheel、retained scene、benchmark）。`python -B tools/check_ant_design_current_baseline.py --self-test` PASS；README 双语、架构、生成 Token 文档和 Gallery 当前入口版本合同通过。
- 新版目录增加一个 retained reference surface；过滤 partial 后为 6 visible / 67 hidden，恢复 all 后 73 项均可见。文档滚动基准为根节点加 73 条目，即 74 个 transform/hit-test 节点；没有因此改变实时样例数。
- 本文件仅证明平台通用的 headless 合同在 Windows 上通过。6.6.5 真实窗口、系统 DPI、GPU/字体视觉截图及 Linux 原生 Wayland 均未在此验收，不复用 change 008 的 6.5.0 证据。
