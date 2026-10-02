# 颜色与变体阶段证据

2026-10-02，Windows x64，MSVC 18 / Ninja Multi-Config。任务 2.1 的平台通用实现已完成；此记录不宣称真实窗口/GPU 或 Linux 验收。

- `windows-msvc-headless-debug`：Button/Slider/Tooltip focused CTest 3/3，3.63 s。
- `windows-msvc-headless-release`：相同 focused CTest 3/3，2.32 s。
- `python scripts/format-code.py --check --clang-format <VS LLVM 22.1.3>`：396 个自有源文件，0 failures。
- `openspec doctor --json`：root healthy，无 status。
- `openspec validate --all --strict --no-interactive`：44/44。
- `git diff --check`：通过。

Button 新合同覆盖 16 色 × 6 变体及 resting/hover/press/disabled/loading；独立选择器优先级、旧 Text/Primary 等兼容路径、ghost Solid/Filled/Link、Theme override 继承/hash/诊断输出与 Material/Geometry/shadow identity 分组。虚线测试检查 outline 及 clip 后的真实透明 gap、颜色更新不测量/不重新 shape、不重跑内容和 Theme dash metric 的 Geometry 更新；非法 enum 在资源获取前拒绝，超过 4096 片段明确拒绝，销毁回调和 over-limit 清理无残留。

开发中发现并修复提前注册空装饰 fragment 改变旧 Flex paint traversal、色板 shadow 被错误归入 color identity、父色板 override 被 legacy 同步覆盖、Filled ghost 保留背景等问题。最终两种配置均通过；完整日志在开发目录 `out/044-variants-tests.log`，此目录不作为提交资产。

图标/形状/ref/loading delay、wave、Gallery 和真实 Windows/Linux 验收仍按后续任务实施。
