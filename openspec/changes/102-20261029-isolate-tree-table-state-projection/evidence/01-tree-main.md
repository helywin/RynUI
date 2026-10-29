# Tree 局部呈现：父 main 实施与测量

日期 2026-10-30。基线 main `033bc96bae1dbe5ee028a04f604fba074ed1cfea`，Tree/Table 五个组件文件与 worker 的 `673d7181` 字节一致；101 已在双方测量中存在。先只导入 fixture/driver，再运行旧生产源码，随后单独导入 Tree 三文件。Table 实现未导入，任务 2.1/2.2/3.1 保持未完成。

## 改动及边界

Tree 缓存 shown/membership，hover/focus 只排入相关行，不触发整树 projection；仅实际 branch activation 标记结构变化。透明背景保留固定 effect 槽位；完整 generation、边界清理及原历史测试保留。新增私有计数只衡量 projection/presentation，不包含 intact、布局或共享 runtime 的访问，不能宣称整个事件 O(1)。

父导入时补齐异常顺序：队列 push 成功后才置 pending bit，全批重绘标志到成功发布后才清除。新增数据缓冲分配故障探针实际比较重试后的全部 packed effects 与正常结果，并核对原代身份。

首轮宽泛全局故障探针命中 MSVC Debug 的 noexcept 空 vector 构造所分配的 iterator proxy，导致运行库 terminate 对话框；已终止测试并保留 CDB 栈和失败日志，不算通过。探针改为仅对至少 64B 的请求注入失败；原 allocation_probe 的默认计数/失败条件不变。测试的 terminate handler 改为直接 stderr/非零退出，不再弹出运行库窗口。这不是任意 OOM 或 noexcept 失败可恢复的声明。16 个注入位置中 Debug 5、Release 8 次实际 bad_alloc，重试字节及身份全部通过。

## 正式构建与回归

Windows MSVC / Ninja Multi-Config / windows-msvc-headless，最多 2 jobs。精确命令、JSON、JUnit 与原始日志在 [windows-tree/index.json](windows-tree/index.json)，脚本 `102-msvc.ps1` 随记录冻结。

- 基线新回归 Debug/Release 各 0/2：Tree hover 重建结构；Table selection 重复排序。基线 Table 数据只作为下一阶段对照，不计 Table 已修复。
- Tree 首次 Debug 44 项通过，新增探针因上述 terminate 被终止；Release 45/45 通过。
- 修正探针后最终 Debug **45/45，12.9558552 s**，Release **45/45，5.567566 s**。包含 Tree 全集、TreeSelect 全集、Focus order/lifecycle、Input component，以及新增 local/invalidation/retry。
- allocation_probe 兼容性补验：重建 glyph_replace/text_scene_service，Debug **2/2，0.5818458 s**、Release **2/2，0.4426289 s**。
- 38 个相关最终 PE SHA、大小、时间戳已记录，默认栈均 1 MiB。未改 API/Theme/金样/renderer/字体/RC/Gallery；未运行原生窗口。

## 同机三轮 Release

每规模每轮独立进程，8 次预热、64 次真实 hover+同步；测量与构建串行。固定 640×600 viewport、Tree 320×180，两行交替实际 HitTest。表格均值为三轮全部 192 样本均值，p95 为每轮各自 p95 的范围，不挑最快轮。

| 节点 | 基线均值 ms | 当前均值 ms | 基线每轮 p95 范围 ms | 当前每轮 p95 范围 ms | 每次 dispatch+sync 分配（前→后） |
| --- | ---: | ---: | ---: | ---: | --- |
| 128 | 1.433644 | 0.077297 | 1.4612–1.5581 | 0.0812–0.1403 | 16+165 → 1+21 |
| 512 | 22.055449 | 0.280016 | 22.5365–24.9281 | 0.2835–0.3101 | 20+549 → 1+21 |
| 1024 | 96.287005 | 0.585031 | 111.4927–124.6010 | 0.6332–0.8166 | 21+1061 → 1+21 |

所有当前热样本均为 projected_rows=0、presented_rows=2、composer_rebuilds=0；旧 composer 每次为 1。旧 host 没有私有行计数，CSV 为 -1，不将源码推导冒充旧运行计数。源与 EXE 指纹、基线二进制身份均保留，基线 Release driver 另保存在 ignored out。原始 UTF-16 日志逐字节保留；UTF-8 JSON/XML/脚本归一化 LF，索引同时记录原始 hash。

这是固定 Tree 的 HEADLESS 局部交互测量，不是整页 Gallery、真实滚轮或 240 Hz present 验收。独立组合审查仍待 task 3.1；Table 缓存、B3 和其余审计阶段继续待办。
