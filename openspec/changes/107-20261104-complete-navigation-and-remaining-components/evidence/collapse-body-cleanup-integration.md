# Collapse body cleanup 主线集成：Linux

- 独立来源：`dee82175bb49e877c879d09c1380416dfcec8bd8`（父为 83 phase，不导入其父链）；仅导入四文件增量。来源 [独立记录](collapse-body-cleanup-linux.md) 不代替下面的 main 重新验证。
- 验证基线：已推送 C `d63acdcc2d447d1aa059ee0250a3b24168beb512`。临时保存并撤下本线程自有、尚未提交的 D 增量，核对工作区恢复干净后再应用修复；没有 Listy、D helpers、83 phase 或其他未交接 WIP。
- 生产修改仅三行：body 建立后、用户 factory 前，以完整 body ticket 注册独立 resource cleanup，直接 Registry false；不经服务层 boundary 通知，不清空/prune Registry，不改变旧 generation/live block/partial retirement/rebind/window isolation/dispose/idle 合同。
- 实际 Linux/GNU，CMakePresets / Ninja Multi-Config；相关构建命令均 `-j2`。实际注册的选择器为 `^rynui\.(collapse_block_cleanup|collapse_component|collapse_contract|component_failure_retirement|modal_component|input_component|select_component|popover_(lifecycle|boundaries)|translation_ownership|retained_translation_phase)$`，共 **10 项**；phase 当前未注册，不计为通过。
- Debug 定向 CTest **10/10**，其中 standalone **1/1、8 fixtures**；0 失败/Not Run。
- 当前 main 最小 Gallery baseline：私有 Mutter/Wayland/独立会话环境，实际 Vulkan/SPIR-V/system fonts；Button 序列 **1 case、12 checks、1 submitted frame**，真实 frame callback/cursor sync。退出调试器审计的两次最终快照均为 **components=0、nodes=0、hits=0、blocked_roots=[]**，分别核对 hit 与屏蔽记录。没有以 hits=0 代替 blocks=0，也未运行未交接 Listy。
- Core include/link 正负 guards、目录 check/self-test **3/3**。
- Release 同一构建 targets 和选择器 **10/10**，standalone **1/1、8 fixtures**；两配置均 0 失败/Not Run。
- `openspec doctor --json` healthy、strict `--all --strict --no-interactive` **107/107**、diff check 通过；独立 main 提交见本文件首次加入的 Git 历史与交付回执。
- clang-format 22.1.3 检查 **1027 文件、0 失败**。任务清单保持 **68/135**；不关闭 D、Table、分类平台或最终收口项。
- 原始日志、调试器逐 ID/generation 审计和 JSON 仅 ignored `out/collapse-body-cleanup-integration/`；未验证 Windows/MSVC/D3D12、物理 IME/AT 或实际跨显示器。
