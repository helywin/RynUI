# Main 集成收口（2026-10-24）

在 main `9c07512b21e60861a9b00f2f471547d5dd4f54ba` 合并 `2425a4c82f18d8e324110749fb79afa5c9ec038f`。保留 main 的弹层滚轮内容优先阶段、空 Table 背景命中、既有 Gallery 和 24+2 Theme 分段解析。新增 Empty/Result 和空 Popconfirm 的零分配 guard。

原 worker 对 tested source `28bee9e744eedf142ceca8c2cf63311346d5cba9` 完整 MSVC headless Debug/Release 各 **148/148**，两个 20,000 次更新场景均为 **0 dispatch / 0 sync allocations**，五份 fresh golden 保留两个基线的旧字段；见 `integration-c3190b0/closure.md`。主分支不重复执行该完整共同矩阵。

父任务使用 `out/typography-dev.ps1 -Preset windows-msvc-headless` 重新构建并运行以下九项，Debug **9/9**（5.40 秒）、Release **9/9**（3.74 秒）：Empty/Result public API、Theme、component，Button component、Popconfirm Theme、Switch features、Theme algorithm、Table component、Dropdown component。精确过滤为 `^rynui\.(empty_result_(component|theme|public_api)|table_component|portable\.(button_component|popconfirm_theme|switch_features|theme_algorithm|dropdown_component))$`，原始输出保存在 ignored `out/empty-result-main-{debug,release}.log`。

clang-format 22.1.3 检查 **583** 个自有源文件、doctor、strict validation **75/75** 通过；清理原 before-fix 日志单行尾部空白后运行 staged diff check。没有新增原生 GPU、Linux 或系统 IME 验收。

Astra R2 在 `2425a4c` 已直接运行 margin、零尺寸、亚像素及 DPI 恢复定向二进制并确认 `layout_size` 修复；用户要求立即收尾时中断了后续 Theme/隐藏分支/回调路径审查。**R2 没有完成，不声称独立审查全部通过。** 已完成的 R1 两项修复、回归与完整共同测试保留。

用户要求停止新增工作。076 Gallery 尚未验收的源码草稿已保存到本机 ignored patch，未合入本提交；075 Form 的已提交模型阶段和未验证 host/Theme 改动保留在其独立工作树。073 的 4/4 共同任务保持完成，不自动 archive。
