# 窗口浮层基础证据

2026-10-02，Windows/MSVC x64，Ninja Multi-Config。

- `windows-msvc-headless` Debug / Release 完整构建，CTest 各 37/37 通过。
- 最终增加 document clip 不得裁掉窗口层的负例；Debug/Release 增量构建与 window_overlay 专项均通过。
- `windows-msvc-debug` 受影响 native CPU 合同 6/6：window_overlay、component_scene_traversal、gallery_document_model、interaction_registry、pointer_route、focus_state。
- clang-format 22.1.3，`scripts/format-code.py --check` 390 文件通过；doctor healthy、strict 40/40、diff check 通过。

已验证普通/浮层子树顺序、祖先隐藏、恢复树顺序、销毁、最近 interaction ancestor、Escape 过滤保留 child focus/正常 Enter。浮层保留 scope/theme/lifetime 归属，通过布局后的共同 hook 定位；Text 与 visible scene 使用窗口层范围。

Gallery 测试不再锁死 partial/planned 数量，验证过滤逐项一致、所有目录条目覆盖、deprecated 数量和已存在 Slider/Input/Typography 状态，修复原有唯一状态数量回归。

此阶段不宣称 Tooltip 已实现或 native GPU/窗口已验收。Linux evidence 独立 pending。
