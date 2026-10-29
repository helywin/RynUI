# 独立复核与收口（2026-10-30）

源码固定为 main `76c469bd5522ee64738183323fafe45c26488904`，直接父 `bced2df3e6456edcab13fa20f33e3a6084f22a04`；累计比较基线 `673d7181bcc116ebc36d2995cf8c8ced35b3eff2`。开始与结束工作区均 clean，两个实现提交已推送并核对 origin/main 的远端 SHA。

独立 Astra（ultra / priority）完整复核 100 容量修复及 101 局部替换，结论 CLEAN，未发现可达 P1/P2。100 R1 的唯一问题关闭：预检与实际分配共用容量计划，288/383B 限制允许 3 glyph，真正超限在事务 begin 前拒绝；epoch、更换能力上限、失败重试与容量内恢复保持。

本轮独立执行的是 ROOT 已有程序：glyph_replace、glyph_scene、glyph_gpu_resources、scene_backend、text_scene_service、input_component、text_reconcile 各 Debug/Release 一次，14/14 exit 0。未构建或运行 benchmark；portable bidi_caret 不存在，按 brief 跳过。96 条冻结记录及 8 个关键 EXE 指纹、时间戳和默认 1 MiB PE 栈均核对通过。

原生测量和配置见 [02-gallery-scroll.md](02-gallery-scroll.md)，容量修复见 [03-glyph-capacity-limit.md](03-glyph-capacity-limit.md)。本轮不重复已通过的窗口测量。父方 23/23 集成与后补资源 3/3 是不同阶段，不把其余 20 项表述为最终 Recording 修改后的重建验收。100 task 2.1 的指定实施、测量及复核闭合；240 Hz 整页体验、Linux、系统 IME/AT 仍为独立未验边界。
