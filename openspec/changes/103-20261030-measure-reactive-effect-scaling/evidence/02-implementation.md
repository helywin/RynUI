# Reactive 与 effect 实施及三轮对照

2026-10-30，用户明确恢复并要求完成本次剩余事项后结束。本轮限定 103，不启动审计 C/D/E/F 或真实窗口测量。起点 main `d6164b6b0bf5a618c6e281d9825079658c97d5be`；暂停备份保留于 out/103-paused-20261030。当前阶段任务为 8/9，独立审查及最终收口尚待 4.2。

## 最终实现

- ReactiveSource 使用有序、可复用 slot 和完整 subscription handle；ObserverNode 保存对应 handle。退订按句柄删除，重新读取依赖时仍追加到通知顺序尾部，不改变 run 开始解绑、get 时登记、A/C/B 重订阅、read 前后写入和动态依赖语义。关联一侧分配失败时撤销另一侧占位，source/observer 销毁和已 dispose 后读取不会复活订阅。
- 修复原 flush 中途 callback 抛错后 current_queue 残留的问题。只释放未执行条目的 queued 标志并清掉当前批次，保留已执行/抛错 observer 排入下一 epoch 的工作，以及其他 phase 的 pending 工作。原 flush_exception 用例在旧实现上 Debug/Release 均实际失败；修复后同一断言及后续 epoch、跨 phase/foreign disposal 回归通过。
- RoundedEffectStore 复用 DirtyRangeAccumulator，首次 getter 才归一化；renderer 对 geometry/material 两个有序 spans 做一次线性归并，保留相邻/重叠并集及原上传内容和事务失败恢复。
- material/geometry 更新先准备 dirty 元数据，再发布 logical/packed 值和诊断计数。新增分配故障回归在旧 effect 实现上两配置均 0/2，明确报 logical update 已提前发布；修复后失败时原值、packed 邻居、dirty spans、identity/index 和计数保留，重试发布成功。

生产改动仅限六份 Reactive/effect 私有实现和头文件。公开 Signal/Memo/Effect API、组件行为、Theme/golden、renderer ABI、shader/RC、字体和 Gallery 未改；不把这次 CPU 微基准扩展为整页帧率承诺。

## 正式执行

环境与 baseline 相同：AMD Ryzen 9 5950X，Windows 11 Pro for Workstations 10.0.26300，MSVC 19.51.36256.0、Ninja Multi-Config，windows-msvc-headless Debug/Release，BUNDLED、HEADLESS/RECORDING。最多 2 jobs；测量时不并发构建，不打开 GUI。

完整脚本、命令、CTest XML、CSV、工具/源码/程序身份见 windows-after/index.json 及同目录原始记录。

| 阶段 | Debug | Release |
| --- | ---: | ---: |
| Reactive 恢复后的定向 build | 2.8123608 s | 2.2992155 s |
| Reactive 定向 CTest | 8/8，0.1862300 s | 8/8，0.1698817 s |
| 最终组合 build | 181.4506021 s | 222.1532054 s |
| 最终组合 CTest | 23/23，2.5518953 s | 23/23，2.4552396 s |

最终每配置 23 个不同合同一次通过，包含两个 benchmark smoke、七组订阅、Signal/Memo/Batch/Scope、source lifecycle、两组 effect dirty retry、effect math/store/scene/gpu/gpu_resources、scene backend 和 Core/backend 边界。Reactive 故障探针每配置实际捕获 14 次至少64B分配失败；effect material/geometry 各捕获 3 次至少64B分配失败。只认可这些有限故障点，不声称任意 OOM 恢复。

正式 after 每种工具三个独立 Release 进程，参数与 baseline 相同：`--iterations 64 --warmup 8 --rounds 1`。Reactive 三轮进程墙钟 0.1962698/0.1963932/0.2044006 s；effect 为 1.5968726/1.5772216/1.5963027 s，包含工具校验/输出，不能代替热更新样本。

## 前后结果与工作量核对

benchmark 两份源码、计量辅助及 allocation_probe 与 baseline 的 SHA256 完全相同。每侧6978个样本，其中6144 hot；逐一核对6978对样本的输入 key、操作/phase、实际 source写入/observer/read/epoch/checksum，或 geometry/material更新数、输入/上传ranges、bytes、packed hash及真实区间，全部相同。32组 hot 每侧各192样本，均值与 nearest-rank p95 从原始样本重算。

以下为三轮 pooled hot，单位 ms；完整五档 K、所有 effect 矩阵/模式、冷挂载与分配在原始 CSV 和 comparison.json 中保留。

| 操作 | before mean / p95 | after mean / p95 |
| --- | ---: | ---: |
| Reactive K4096 stable active | 171.956106 / 181.8520 | 0.147261 / 0.1597 |
| Reactive K4096 dynamic switch | 257.504446 / 268.9908 | 0.207128 / 0.2357 |
| Reactive K4096 dynamic active | 342.597030 / 362.7702 | 0.205226 / 0.2306 |
| effect 10k/1000 material update（含首次 dirty 读取） | 2.414829 / 2.5637 | 0.044210 / 0.0571 |
| effect 10k/1000 material 完整 sync | 2.241141 / 2.3046 | 0.210135 / 0.2353 |
| effect 10k/1000 geometry update | 2.372416 / 2.5247 | 0.068206 / 0.0805 |
| effect 10k/1000 geometry 完整 sync | 2.192401 / 2.2764 | 0.208747 / 0.2342 |
| effect 10k/1000 overlap update | 5.437189 / 9.0364 | 0.100082 / 0.1170 |
| effect 10k/1000 overlap 完整 sync | 7.716267 / 13.0953 | 0.167934 / 0.2135 |

K1 stable mean 为0.0000546875→0.0000567708 ms，两个 p95 均0.0001 ms；不宣称小输入普遍提速。新 slot/handle 用额外存储换取直接解绑：K4096 fresh graph 的 C++ new 请求字节，stable 为1,174,590→1,758,459，dynamic为1,499,510→2,732,712；这不是峰值RSS或常驻内存。该取舍在全量冷样本中保留，没有隐藏冷挂载成本。

effect hot update 仍包含首次 dirty getter 的归一化；完整 sync 包含规划、打包与 Recording enqueue，commit 单列。内部 subscriber 扫描、sort比较与纯 planning 未被直接观测，不能把推导复杂度写成实测计数；实际上传ranges/bytes未减少或退化为 full upload。

## 历史失败及证据边界

- effect 新测试初次构建缺少公开 include 目录，失败日志保留；仅补齐测试目标 include 后重建，随后才形成旧 effect 0/2 的有效失败证据。
- 首次全局 format 命令未指定本机 clang-format 路径，工具未启动；传入显式22.1.3入口后全仓813份源码零失败，新文件另行 dry-run 通过。
- 既有依赖的 C4819 和 HarfBuzz CMake 提示保留在原始构建日志，不冒充测试错误。
- 此处为本轮正式新构建、HEADLESS/Recording 合同和同机微基准；未运行 native窗口、GPU/driver、Linux、系统IME、AT或整页240Hz验收。审计其余阶段仍待办。
