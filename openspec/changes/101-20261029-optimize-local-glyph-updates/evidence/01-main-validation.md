# 101 main 实施与验证

## 身份与改动

原始局部更新基线为 `673d7181bcc116ebc36d2995cf8c8ced35b3eff2`；100 容量上限修复已独立提交并推送 `bced2df3e6456edcab13fa20f33e3a6084f22a04`。worker 的源码交接单保留原 source-only 状态；本记录为父线程随后在 `D:/code/RynUI` 实际构建与执行的验收。

等长非空替换先为 dirty 元数据准备空间，再执行允许重叠的局部 memmove，最后追加原 range。错误输入检查、空操作及原变长路径保持。测试覆盖 self/双向 overlap、邻居内容与身份、material/geometry dirty、真实分配失败及恢复；真实 Text 内容变化也验证前后邻居的 instance/draw/line ranges。

RecordingRenderer 增加仅用于设备模拟的 `reset_device(capabilities)` 重载：仍先完成既有 epoch 退役，再更换能力。用于验证新设备上限变化；不修改 SceneBackend、SDL、GPU ABI 或公开 UI API。

## 先失败，再修复

旧生产实现配新测试，Debug/Release 各 5 项中 3 项真实失败：局部替换重建整 store、真实 Text 内容更新重建 store，以及 100 的有界 Glyph 增长失败。原始 stdout、JUnit、进程 exit/计时均保存。然后只修复 100，资源三项两配置通过；随后导入 101 等长实现，原五项两配置全部通过。

旧版和新版采用相同 benchmark 源码、`--iterations 200 --rounds 3`，分别使用 `--baseline` 与正常模式。旧版模式要求实际观测到全 store 分配及地址变化，防止误把新实现作为旧版；四种 source 模式与三种背景规模全部保留。旧版 EXE 已被增量构建替换，未保留其 hash，基线对应性依赖固定旧源码、构建日志、模式守卫及原始运行记录，不声称可重新核对旧 EXE 文件。

## 三轮局部更新基准

Windows MSVC / Ninja Multi-Config / HEADLESS。下表为 Release external 模式，各轮每次替换 4 glyph，200 次更新；所有轮次保留，不选择最好一轮。

| 背景 glyph | 修复前每轮均值 ns（三轮） | 修复后每轮均值 ns（三轮） | 200 次申请次数，前→后 | 200 次申请字节，前→后 |
| ---: | --- | --- | --- | --- |
| 1,000 | 1598.5 / 1686 / 2194 | 22.5 / 24 / 24.5 | 200 → 0 | 18,407,800 → 0 |
| 10,000 | 24552 / 22907.5 / 21733.5 | 24 / 25.5 / 23 | 200 → 0 | 184,007,800 → 0 |
| 100,000 | 1765480 / 1827200 / 2036600 | 26.5 / 22 / 22.5 | 200 → 0 | 1,840,007,800 → 0 |

分配字节为 operator new 的真实请求，含 allocator 额外空间。冷路径仅申请 8 字节 dirty 元数据，热路径零分配、地址不变。当前平台单次时钟读数有约 100ns 粒度，修复后的 p50 可为零，不能把这些均值解释为精确单次延迟、GPU 加速倍数或 Gallery FPS。

glyph CPU stride 为 92 字节。源码推导的复制范围上界由整个背景（92,000 / 920,000 / 9,200,000 字节）变为 368 字节，**不是运行时 memcpy/memmove 计数**。实际分配、完整内容比较和 store 地址另行测量。Debug、四模式、p50/p95 与所有原始轮次见 `windows/101-{before,after}-bench-{Debug,Release}.log`；汇总算法和字段见 `windows/benchmark-summary.json`。

## 持续增长、能力与失败恢复

`rynui_portable_scene_backend.exe --glyph-growth-counts` 每配置三轮：逐个追加 1,000 glyph，均为 11 次分配、容量 1,024、1,000 次上传、194,208 字节；每个已提交 glyph 的实际 packed bytes 与逻辑值比较。删除末尾 100 后恢复保持 handle；epoch 重建一次分配且旧资源不重复释放。

最终每轮另覆盖已稳定的 1,000 glyph 背景，再逐个追加 1,000：含初始背景总计 2 次分配（追加阶段 1 次）、最终容量 2,000，追加阶段 1,000 次上传、192,000 字节；同样检查全部前缀、删除恢复及变化设备能力。两模式三轮的全部计数均一致，原始记录为 `windows/101-background-growth-{Debug,Release}.log`。

新增设备能力用同一个 RecordingRenderer/SceneResources 实际切换：旧 1,024 容量设备退役，逻辑缩至 3 glyph、上限改为 288 字节，合法内容成功且容量为 3；增加至 4 glyph 在 begin 前拒绝并保留故障注入；再次改为 384 字节设备后重试成功。补齐旧能力不得跨 epoch 沿用的合同。

288/383 字节边界及 create/upload/commit 失败恢复复用 100 已新增回归，未撤掉原断言。

## 新构建与实际测试

驱动为 `windows/101-msvc.ps1`，正式 CMake presets，最大 2 jobs。各阶段顺序执行，benchmark 与构建不并发；未开窗口。

| 阶段 | Debug | Release |
| --- | --- | --- |
| 局部实现 targets 构建 | exit 0，4.5524553s | exit 0，5.0240651s |
| 局部五项 CTest | 5/5，1.1307435s | 5/5，0.7945797s |
| 受影响集合构建 | exit 0，31.5237239s | exit 0，34.5032199s |
| 受影响集合 CTest | 23/23，42.7271945s | 23/23，4.6606413s |
| 增补变更能力后资源构建 | exit 0，11.1269972s | exit 0，10.9544661s |
| 增补变更能力后资源 CTest | 3/3，0.5827863s | 3/3，0.416555s |
| 补齐非空稳定背景后资源构建 | exit 0，11.3123571s | exit 0，13.394042s |
| 最终资源 CTest | 3/3，0.5972126s | 3/3，0.4162919s |

23 项包含 Glyph replace/scene/GPU、Text scene/render/state/editor/history/reconcile/caret/clipboard、Input component/GPU/display/caret、Button、Quad、SceneBackend、Reactive source lifecycle、Bidi analysis/caret 和 backend boundaries。共享 allocation probe 保留原 count/fail_at，所选既有分配/失败测试通过；未宣称全部使用该 header 的 native-only 目标或完整 CTest suite 已重跑。增补能力之后只重建、重跑受影响三项及增长计数，不将其余 20 项描述为最后一次源码变化后的重跑。

原始日志、JUnit、进程结果和当前源码/8 个 EXE 指纹保存在 `windows/index.json`；8 个 PE 均保持默认 1 MiB 栈。UTF-16 原日志逐字节保留，UTF-8 JSON/XML/脚本按 Git 约定归一化 LF，索引同时记录原文件 hash。100 的早期索引也补此归一化说明，未改写原运行结果。

clang-format 22.1.3 全量 806 文件、0 failures；显式 OpenSpec 1.14.0 doctor healthy、strict 102/102、apply 回读 3/4；diff 检查通过。

task 1.1/1.2/2.1 已完成；2.2 的独立审查和最终提交关联待办。本记录不代表新的 Gallery、真实 GPU/原生窗口、240Hz、Linux、系统 IME/AT 验收；审计报告后续阶段继续待办。
