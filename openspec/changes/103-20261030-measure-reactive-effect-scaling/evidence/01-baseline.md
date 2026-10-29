# B3 正式基线与最小方案

2026-10-30，ROOT main 起点 `c7cb0950e5f66966180436f2e5d17b785a11350b`。本阶段只增加/修正测量与回归输入，生产 src/include、组件、Theme、golden、Gallery、RC/字体保持该提交原文。源码交接的未编译边界见 `source-handoff.md`；本记录属于父线程实际执行。

## 环境和命令

Windows 11 Pro for Workstations 10.0.26300，AMD Ryzen 9 5950X（16核/32逻辑处理器）。官方 `windows-msvc-headless`，MSVC 19.51.36256.0、Ninja Multi-Config、BUNDLED、HEADLESS/RECORDING；最多2 jobs，构建和测量串行，未启动 Gallery 或执行输入操作。

完整命令脚本为 `windows-baseline/103-msvc.ps1`。configure 20.0604894s；初次 build 命令把 Python backend_boundaries 合同误当 EXE target，立即失败并保留日志，随后删除不存在的构建目标（CTest合同仍保留）。正式 Debug build 224.1999605s，13/13合同通过（阶段2.5310409s）；Release build263.2506299s，13/13通过（2.5053859s）。新增 Signal 和六项订阅顺序/生命周期/故障回归分开构建执行：最终 Debug build1.6020284s、7/7通过0.1810278s；Release build1.6884925s、7/7通过0.1774703s。每配置共20个不同合同通过，不冒称一次完整矩阵。

Release 先执行完整矩阵的1个热样本/0预热以估算时长，单独标记 sizing，不混入 baseline。正式每种测量3个独立进程，每进程 `--iterations 64 --warmup 8 --rounds 1`；全部结果和退出码保留。Reactive三轮进程墙钟59.0893014/59.3231974/59.1490804s；effect三轮3.5158742/3.5250984/3.5835783s。

## 有效性和口径

baseline共有258组汇总、6978个原始样本，其中6144个热样本；另有 sizing 的54个样本。`103-summarize.py` 逐组重算总量、nearest-rank p95，并核验实际 callback/写入及上传区间。原始CSV和 JSON 均冻结，不只保存最好一轮。

Reactive覆盖K=1/64/256/1024/4096、稳定依赖、动态switch/active/inactive。每个 observer 的输出与次数检查位于计时外；inactive确实0回调，不混入活动更新均值。冷挂载单列，包含新graph但不代表冷进程/cache。

effect覆盖(1000,256)/(10000,256)/(10000,1000)，四种material/geometry/mixed/overlap。所有热上传仍为对应数量的非相邻单实例区间，没有full upload/重分配/compaction。逐项校验logical/packed值、未改邻居、实际提交bytes和hash，上传失败/commit失败/epoch回归在计时后运行。

父方在首次baseline前把 **hot update及首次dirty getter** 一起计时，防止后续lazy归一化被区外校验提前执行而漏计；cold_sync仍只计同步。effect_sync含规划、打包与Recording enqueue，commit单列；这些是CPU/Recording成本。subscriber scans、sort comparisons、纯planning时间仍不可观测，CSV留空。分配为被测线程C++ new请求，不是峰值内存或全进程分配。

## 实测与决定

以下均为三轮热样本的合并平均毫秒，p95及全部规模/模式在原始CSV和summary中，不以均值代替尾延迟：

| Reactive K | stable active | dynamic switch | dynamic active |
| --- | ---: | ---: | ---: |
| 64 | 0.029259 | 0.043994 | 0.057707 |
| 256 | 0.456880 | 0.690266 | 0.912532 |
| 1024 | 9.503867 | 14.277411 | 19.174440 |
| 4096 | 171.956106 | 257.504446 | 342.597030 |

相同一次Signal变化实际触发K个observer，而总耗时增长明显快于K。源码确认每个run退订/重订阅均扫描source订阅表；这是实测规模曲线与源码共同支持的归因，不是实测扫描次数。选择有序可复用subscription slots+dependency handle，保留原先run开始解绑、get时尾插的语义。不得以朴素保留旧依赖改变read前写入和通知顺序。

| effect存量/ranges | 模式 | update（含首次dirty读取） | 完整sync |
| --- | --- | ---: | ---: |
| 10000/256 | material | 0.143021 | 0.142427 |
| 10000/1000 | material | 2.414829 | 2.241141 |
| 10000/256 | overlap | 0.281897 | 0.388589 |
| 10000/1000 | overlap | 5.437189 | 7.716267 |

固定存量下离散ranges增加产生明显成本，源码存在store每条排序与renderer每条重复排序两处。选择复用DirtyRangeAccumulator，并把renderer两个有序spans一次归并；保持全部真实ranges/bytes/packed内容和事务失败恢复。生产改造前后使用同一harness与采样口径。

## 限制与历史失败

新增原始分配探针使用无大小过滤的0..7注入，Debug运行库错误对话框使allocation case未正常结束；父仅结束该测试进程并关闭窗口，保留失败阶段记录。最终探针在注入前预置1..16个外部observer，0..8故障点仅让至少64B请求失败，两配置各实际捕获10次bad_alloc；验证旧目标清理、外部observer存活与健康新observer可用。本EXE诊断定向stderr且保留失败退出；未改共享probe。该有限覆盖不扩展成任意OOM恢复。

已有flush中途callback异常的队列残留风险由源码检查识别，当前初次异常用例不覆盖它；后续改造须明确复核，不把首次异常通过扩大为全部异常恢复。没有GPU、native、Linux、IME/AT或240Hz体验结论。
