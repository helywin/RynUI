# Typography 多行编辑：平台通用验证

2026-10-23，Windows 11，MSVC 19.51.36256 x64，Ninja Multi-Config，BUNDLED。平台通用验收使用 `windows-msvc-headless` Debug/Release，运行实际共享 Core、组件、文本与 Recording 后端。

- 最终 Debug 完整 CTest **99/99**（231.13s），Release 完整 CTest **99/99**（10.50s），均包含最后的 Input shell 缓存修复、Escape 分发修复和对应回归。Debug 的输入场景分配基准也通过，没有以首轮/修复前结果替代最终结果。
- `windows-msvc` Debug 的 Gallery generator/catalog/document model/document contract/frame/token-gallery contract **6/6**（54.36s）。这些是共同逻辑与 Recording fixture，不据此宣称真实 GPU 通过。
- clang-format 22.1.3 全仓 459 个自有源文件检查0失败；OpenSpec 1.14.0 doctor healthy、strict 57/57、`git diff --check` 通过。Gallery catalog `--write` 后 `--check` 通过。

验证覆盖：保留原文换行、真实多行 TextArea、Shift+Enter 插入与 Enter 提交、受控回写/拒绝、maxLength、autoSize 行数限制、动态标题字体与方向、取消/blur/禁用/销毁与 identity；普通 TextArea 的 Enter 仍换行。其他组件 hover 提示存在时，Escape 先取消 IME，再取消草稿；未被文本编辑消费的 Escape 仍关闭 Tooltip。重新进入编辑后检查 shell effect 恢复可见。

同步修复两处已存在的验收漂移：Gallery Button 调用点在远端基线 `12100f1` 已为26，旧检查仍为18；Glyph 锁定源码与实际 reflection 已为6个 float4（含 TEXCOORD5 rotationBasis），旧检查仍为5。未修改 shader、GPU ABI 或依赖版本。五个新增 Typography Tooltip 的 trigger/popup 与隐藏 popup 分别纳入 Gallery interaction 数量合同。

用户文档、README 能力入口、收尾清单与 Gallery supported/missing scope 均已同步；保留富文本与完整 Web API 未覆盖状态。Windows 原生证据单独记录；Linux 原生与人工系统 IME 不由本文件验收。
