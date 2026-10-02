# Space 集成验收（平台通用）

2026-10-03，实际使用 Windows/MSVC。

- `windows-msvc-headless-debug`：构建完成，全部 CTest **71/71**，97.09 秒。
- `windows-msvc-headless-release`：构建完成，全部 CTest **71/71**，18.13 秒。
- `windows-msvc-debug` 的 `rynui.token_gallery_frame`：**1/1**，23.19 秒。此测试使用记录 GPU API 验证共同 Gallery 合同，不替代真实 GPU。
- Gallery 新增 8 个稳定标识、11 组 Space 样例，当前 110 stable IDs / 129 live samples；内容与 Theme 不重新挂载，新增 21 Buttons、4 editors、1 RadioButton 及其实际交互库存按最终结构核对。
- support-overlay 的 Space 原生功能标为 implemented，生成器要求 050 实现证据；删除不存在的 responsive API 缺口描述，Web 属性保持明确排除。生成器 `--write`、`--self-test`、`--check` 通过。
- clang-format 22.1.3 格式检查、OpenSpec doctor/严格校验与 diff check 通过。

HEADLESS 增加现有 password/search/input public fixtures，覆盖 CPU-only 组件和公开头合同。所有新增组件继续通过实际 HEADLESS backend/include/link 边界检查。

Windows 真窗口/驱动/shader/系统字体/input/DPI 结果单独记录在 Windows 证据；Linux 平台项保持独立待验收。
