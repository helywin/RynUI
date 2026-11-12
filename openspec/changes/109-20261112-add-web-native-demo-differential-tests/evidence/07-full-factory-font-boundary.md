# Full-factory Web 执行与实际字体边界

Linux，独立 locked Chromium 1194 / 141.0.7390.37，原 120 秒 **每次 Web execute** bound，串行执行、fresh out。窗口冻结 app SHA `94f607796678470f518c7919d10773d03eb4d403202a479c20bb55b47ee34388` / Web closure `1f669e170563ed787da084770b8beeca9ef6492f54861dd33cbd4c1948012b0a` 与三份 request；执行前后 hash 一致。该 full-factory 代码仍为自有集成 WIP，精确 source 副本和 patch/hash 仅 out，不称已发布 adapter 的新 paired 通过。

- 共同协议差量后的现有 Web methods **18/18**，189.996 秒；包括五个官方 exports、真实输入/负例、来源/缓存/字体/自然高度/完整组合上下文。所有正例现经主 runner 的严格 observation reader 校验。
- 原 Button 定义完整 factory：**18/18 actions、5/5 checkpoints、hello/complete、2 callbacks、独立 Web oracle 0 fail**，10.421 秒。公开 Flex/Space/Typography composition 与完整 authored context 保留。
- Input revision2（显式 UTF-8/UTF-16 单位）在实际 `accept-edit` 后的 accepted checkpoint 因 `unexpected font source/fallback: gallery.input.controlled` **rc1**。已完成 **5/14 receipts、1/5 checkpoints**，无 complete；失败不算 pass。原 11-vs-4 定义 red 仍独立保留，此处不是同一失败或产品修复。
- 立即停止窗口，无重试；Select **not-run**。自有 execute/Chromium PID 清单均为空，GPU RELEASED。无 native、新矩阵或用户桌面/浏览器操作。

CPU 独立读取两份实际 pinned font 的 cmap 并以 fc-query 交叉核对：均不覆盖 U+1F642。旧 native accepted 图对应 metadata 的 byte cluster7 是 Noto Sans glyph568，其 cmap 映射为 U+FFFD；TextEngine 的缺字替换路径与此一致。此前以 `replacement_count=0` 推断 emoji 覆盖的解释撤回：该字段只数 UTF-8 repair。Web throw 前未持久化失败 face 的明细，因此不猜测具体系统 fallback face。

本小阶段仅为 Gallery 增加既有 FontRuntime nominal-glyph 只读查询，显式导出 UTF-8 repair 单位及缺字 scalar，保留 Core 默认行为。正式 `linux-gcc-headless`、Ninja Multi-Config、单 build `-j2`、配置串行；受影响 native input CTest Debug **1/1**（4.25 秒），Release **1/1**（1.28 秒）通过。真实 `abc🙂` 回归同时断言 UTF-8 repair0 与 U+1F642 缺字，不以文字字符串/非零 glyph ID 证明视觉正确。

当前两文件字体准备不足以完成同文本 paired pilot；没有删除 emoji、使用任意系统 fallback、放宽 font gate 或更新 reference 消红。三例全 factory、完整输入映射、有效校准 profile、分层 AND 比较、真实 fault 红绿及 Windows 均继续开放。此阶段不新增 checkbox，109 **15/61**；原始失败、图、日志、字体/cmap 资料、源码副本及运行指纹仅 out。

clang-format 22.1.3 检查 **1228 files / 0 failures**；OpenSpec 2.0.0 doctor healthy、全 strict **109/0**、diff check 通过。相关 `cmake --build --preset linux-gcc-release --target rynui_token_gallery --parallel 2` 编译通过。当前 binary SHA `2936321180a05277032e9de6e2799fc851d8cf6e0c94c31cce95437bb5b2280a` / closure `9961566e2d54ff9be6e3b85b2b808cc35c85108f8b78e3f4ffaf054a5cb8c66d` 仅 CPU build，未追加 GPU 运行。
