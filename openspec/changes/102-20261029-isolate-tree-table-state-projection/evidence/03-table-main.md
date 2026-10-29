# Table 投影缓存与分页重排：父 main 验收

2026-10-30。直接基线 main `f595211e753f8d117dd72f1390183515a9577b31`；原 Table 性能对照在 `033bc96b` 测量，其 Table 实现与 `673d7181`/f595 字节一致，双方均已有 101 的 glyph 优化。Table fixture 不挂载 Tree。worker 只交付源码，本记录的构建、测试和测量全部由父 ROOT 执行。

## 实现与实测驱动的修正

Table 按数据/排序 revision 缓存完整排序，分页只生成切片；selected membership 使用有序集合。纯选择、bordered、翻页均不调用 comparator，也不复制源 records。shared state 保活与 pending refresh/revision 检查允许 comparator 更新数据/排序/页码、销毁原 owner 或重用 index；不提交旧投影。

父集成新增并实际复现了两个失败边界：

- 初轮四项新回归 Debug **3/4**，富单元格 binding getter 销毁 Table 后，Text 继续写旧 TextSceneId。Text content 交付增加原完整 scene generation 存活检查，原回归继续执行，未删除销毁场景。
- 长 key 的真实分配故障导致排序 revision 已提交、sorted_order 只填入部分行；随后 bordered 重试丢行。修复前 Debug exit 1，修复后以临时 vector 完整构造再替换，order dirty 与未完成 geometry 意图保留。最终 Debug/Release 各实际捕获 **24 次 bad_alloc**，比较完整行序、真实坐标和 retained IDs；另覆盖 comparator 原异常与正常属性触发的重试。只注入至少 512B 的请求，不声明所有 OOM/noexcept 分配可恢复，也不自动立即重试。

第一版缓存的三轮结果没有掩盖分页瓶颈：万行翻页均值仍为 **268.402096 ms**。源码定位到每次翻页重排全表交互，而共享 reorder_after 有逐项线性去重/查找。新增声明顺序回归修复前 exit 1；Table 改为按完整 sorted_order 排列 retained 后代，仅数据/排序改变时重排。分页、disabled/loading 由已有 branch eligibility 筛选当前可聚焦项；没有修改共享 Registry/Focus 算法。真实 Tab/Shift+Tab、外部邻居、分页恢复、增删排序及 editor/session/draft 身份仍检查。不扩展为任意 append_slot 动态插入均已测试。

## 正式构建与合同

Windows MSVC / Ninja Multi-Config / `windows-msvc-headless-{debug,release}` / HEADLESS / RECORDING / BUNDLED，最多 2 jobs。构建、测试、benchmark 串行，无原生窗口操作。

- 缓存初版修正后两配置各 **26+21=47** 个不同合同通过；其后才依据实测继续消除翻页重排。这一中间阶段的源码/EXE/hash/原始记录冻结在 [windows-table-cache/index.json](windows-table-cache/index.json)。
- 最终生产代码 Debug 主集合 **25 通过、1 夹具失败**（300.0106115 s）。新增反向 Tab 已回到 before Input，再次 request_focus 返回“未变化”；夹具错误地把返回 bool 当作成功。只将该检查改为核对实际 focus ID，前后向完整路径、草稿、代际断言保留；重建后该项 **1/1，0.2165174 s**，关联 Tree/TreeSelect **21/21，2.9702168 s**。合计 47 个不同合同均有最终生产源码通过记录，但不能称为最后一个 Debug EXE 一次运行 47/47。
- 最终 Release 重新编译/链接 **23.2634103 s**；主集合 **26/26，38.9913871 s**，关联 **21/21，1.7978998 s**，合计 47/47。
- Debug 主集合包含最终 retry 用例、Tree 局部呈现/失效/故障、Table 全集、Pagination、Focus/Input、Text component/frame/scene。新增反向路径的全部旧正向断言未变。早期“preceding Input focus failed”日志及修正前 source/EXE 指纹保留，失败不计通过。
- 最终索引记录 **34 个 EXE、8 个 OBJ、2 个 LIB** 的 SHA256、大小与 UTC mtime；PE 栈均为默认 **1 MiB**。953 个原有 source/build/test/asset 文件的 Git blob 保留；允许改动仅 Table 两文件、Text 四行 guard、专项测试和 CMake 注册。公开 API、Theme/金样、renderer、RC、系统字体、Gallery 及 Tree 生产文件均保持。

精确阶段参数、JSON、JUnit、原始输出及脚本见 [windows-table/index.json](windows-table/index.json)。典型复用命令如下；历史各次 Label/Stage/Configuration/Count/Round/Iterations 均保存于同名 JSON，不能将失败历史或中间结果改称最终结果。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File out/102-msvc.ps1 -Configuration Release -Stage table-build -Label table-page-final
powershell -NoProfile -ExecutionPolicy Bypass -File out/102-msvc.ps1 -Configuration Release -Stage table-test -Label table-page-final
powershell -NoProfile -ExecutionPolicy Bypass -File out/102-msvc.ps1 -Configuration Release -Stage table-related-test -Label table-page-final
powershell -NoProfile -ExecutionPolicy Bypass -File out/102-msvc.ps1 -Configuration Release -Stage bench -Label table-page-final -Component table -Count 10000 -Iterations 64 -Round 1
```

## 三轮 Release 热更新

每规模每轮独立进程；每操作 8 次预热、64 次真实 Signal 更新加完整 layout/synchronize，viewport 640×600，pageSize=10。三轮全部 192 样本的均值；p95 为每轮 p95 范围，未挑最快轮。冷挂载、预热和析构不计入热样本；进程完整耗时另有 JSON。所有原始轮次保留。

| 行数 | 操作 | 原均值 ms | 最终均值 ms | 原每轮 p95 ms | 最终每轮 p95 ms |
| ---: | --- | ---: | ---: | --- | --- |
| 1000 | 选择 | 0.416177 | 0.375348 | 0.4026–0.7737 | 0.3905–0.7698 |
| 1000 | 边框 | 0.655323 | 0.601413 | 0.6841–1.0318 | 0.6981–1.0289 |
| 1000 | 翻页 | 3.626896 | 0.609089 | 3.8387–4.2929 | 0.6419–1.0082 |
| 5000 | 选择 | 5.582576 | 3.784416 | 4.5693–12.2997 | 3.4100–11.4737 |
| 5000 | 边框 | 5.589685 | 4.897148 | 6.5661–7.6887 | 4.9612–7.4868 |
| 5000 | 翻页 | 77.301969 | 4.379793 | 90.3261–110.7141 | 5.0189–5.8585 |
| 10000 | 选择 | 16.214690 | 14.544443 | 20.2402–28.1782 | 24.1970–25.6301 |
| 10000 | 边框 | 24.623315 | 19.054695 | 34.8313–35.3443 | 25.8512–33.7820 |
| 10000 | 翻页 | 266.697498 | 25.492305 | 308.4986–341.7031 | 35.5577–39.2026 |

实际 comparator 每次由 1k 的 3,421、5k 的 22,159、10k 的 49,311 降为 **0**；三种热操作的 copied_records/sort_passes 同为 0。分配、visited_rows、composer_rebuilds 的逐样本原值见 summary 与 CSV；旧私有 work 计数缺失以 -1 表示，不冒充已测旧访问量。中间缓存版的全部数据也保留。

选择/边框的耗时改善有限且存在波动，不能只凭零 comparator 宣称整体常数成本。最终仍会逐行更新状态/几何，边框可能触发 scene 重建；数据/排序变化仍需全量重排交互。万行翻页虽明显改善，仍远超 240 Hz 的 4.17 ms 帧预算。这些是 HEADLESS 固定夹具，不是 Gallery 实际滚轮、GPU present 或高刷体验验收；后续 B3 与阶段 F 继续测量通用 runtime/scene 和实际显示链路，不能用未来虚拟化代替当前局部优化。

格式检查使用显式 clang-format 22.1.3，809 文件零失败。首次 PATH 缺少 clang-format 的诊断保留；修正为显式路径后通过。显式 OpenSpec 1.14 doctor/strict 与 diff 检查在收口再次执行。task 2.1/2.2 以本阶段证据完成，3.1 的独立组合审查仍待办。
