# Gallery 实施前 Core 冻结基线

2026-10-26，task 1.2 已满足。三个依赖分别独立审查、完成 main 实际新构建，并推送核对远端精确 SHA：

| 依赖 | 独立审查 | main 发布 SHA | main 新构建 MSVC HEADLESS Debug / Release |
| --- | --- | --- | --- |
| 088 Avatar / AvatarGroup | Astra R2 未发现可达 P1/P2 | `6118f67728aede9985867377b122274ac86f31d7` | 各 18/18 |
| 090 Rate | Astra R3 未发现可达 P1/P2 | `528eed92ae66977be64cba8f5d191e90824aff47` | 各 29/29 |
| 089 Statistic | Astra R7 未发现可达 P1/P2 | `29154cda31f3a8e7c5da06b660182fc75a2697fc` | 各 45/45 |

共同源码冻结基线：**`29154cda31f3a8e7c5da06b660182fc75a2697fc`**。它已包含三个公开 headers/hosts、完整 Theme 链、所需保留输入/窗口层及实际组合测试，不含未审 AutoComplete 或 Tree。093 实施不得修改 Core、公开 API、Theme、goldens、renderer 或 CMake；若发现核心缺陷，先由所属 change 修复、发布，再明确更新基线，不能在 Gallery 中掩盖。

对应可复核命令、原始日志、金样深比较和边界分别见 [088 main 验收](../../088-20261026-add-native-avatar/main-integration.md)、[090 main 验收](../../090-20261026-add-native-rate/evidence/main-integration.md)、[089 main 验收](../../089-20261026-add-native-statistic/evidence/main-integration.md)。这些是组件与共享合同的 HEADLESS 验收，不代表交互 Gallery 已实现。

Gallery 保留性对比基线仍为 `6118f67728aede9985867377b122274ac86f31d7`。实际 Git 比较确认 `examples/`、`gallery/` 和 `tests/token_gallery_frame_tests.cpp` 从该基线至 `29154cda` 无变化；实施前再次源码计数为 **36** 个 `void add_*` 样例函数。390 旧 IDs/相对顺序、36 旧函数体/调用顺序和 70 非目标 overlay 条目须由实施后的比较脚本验证，不将规划计数代替运行 inventory。三目标 support 条目在真实流程验收前继续保持 planned。

本阶段只记录 task 1.2，2.1、3.1 保持未完成。后续新样例使用平台通用 Windows MSVC `windows-msvc-debug` Recording、新 `--members-metrics-only` 和三主题双宽；完整 Gallery/native 在用户指定收尾时统一执行，不扩大为 GPU/人工视觉、Linux、系统 IME 或 AT 验收。
