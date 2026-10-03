# 2.1 逻辑 shaping 共同验收

2026-10-03，实际 Windows/MSVC，`windows-msvc-headless`，Ninja Multi-Config，BUNDLED。

Debug/Release affected CTest 各 5/5：`bidi_shaping`、`portable.font_runtime`、`portable.text_engine`、dependency lock、backend boundaries；耗时 1.80s / 1.94s。

新增 fixture 使用锁定 Noto Sans Arabic 2.009 / Hebrew 3.000，字体 name table 核对版本，来源同 Noto Sans 的精确 commit。字体只保存在 build tree，lock 集中 source/hash/license；SYSTEM 显式提供兼容 fixture。

实际回归涵盖混合 Arabic/Hebrew/Latin/CJK、font/level/script runs 的逻辑覆盖、每 run cluster 方向、真实 Arabic full-source 邻接上下文/ligature、括号 mirror、控制符无 replacement/无可见 advance、真正缺字原始 cluster、显式 RTL 的 LTR 数字、非法方向和 CRLF/U+2029/末尾空段。原有 mixed-direction rejection 回归已更新为 RTL run 合同。

clang-format 22.1.3：454 自有 source files，0 failures；doctor healthy、strict 55/55、diff check 通过。Release 构建的既有 Button nodiscard 与若干 viewport/theme unused warnings 保持基线，新增 shaping 源码无该类 warning。

此阶段保持 ShapedText 的逻辑 run/glyph 数据；逐行视觉重排、光标、selection 和真实 native window 尚属后续任务。
