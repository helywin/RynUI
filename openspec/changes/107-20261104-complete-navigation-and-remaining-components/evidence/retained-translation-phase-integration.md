# Retained translation phase 主线集成：Linux

- 仅导入来源 `83e6fa6ba28fef432a7652f7004ebfe62991c050` 六文件差量，基线 main `e05c864366353c2f1470a7eea5c13c73d91d859a`。A=c801、Avatar1a=695 三文件 helper 与 dee cleanup 已交付，均不重复导入；不含 Listy/Table feature 或未提交来源。
- 全部 window layer writer 后、glyph 同步前收集确切 owner generation 的 callback，按真实祖先深度统一执行。重复/非收集期入队拒绝，stale 跳过，compose 异常精确退役 owner；不增加竞争 phase/flag、reset/restore 循环或轮询。
- 实际 Linux/GNU，`linux-gcc-headless`、Ninja Multi-Config，相关 targets `-j2`。最终 Debug **14/14**、Release **14/14**；其中 `retained_translation_phase`、Runtime-only `translation_ownership` 各配置均 **1/1**。相关 selection/Modal/Tabs/Input/Select/Popover、Collapse 及失败退役、J Form scope 保留通过。精确 selector 和命令仅保留于 ignored out。
- phase fixture 实际验证正/反 participant attach 顺序、晚到祖先 writer、嵌套 root+local 组合与父级更新、真实 glyph/Input 位移/焦点、重复/stale/异常及 idle；primitive 保留默认/销毁/slot reuse 和 descendant preservation 合同。没有用这个结果声称未来 Table/Listy 组合已通过。
- 收到其他任务误复用 HEADLESS dependency BUILD 目录通知后，没有用 mtime 宣称一致。待自有构建退出，在独占点按主线 preset 重新 configure，补建并重跑同一 14 项。实际日志显示 Debug FreeType/UTF8Proc/SheenBidi/HarfBuzz 库均重新构建、相关 executable 重链；记录实际生成头/两配置库内容 SHA 和编译命令图，两配置均无其他 worktree 的构建路径。首轮与最终重复集合不累加；既有冻结 native binary 证据不撤销，未扩大为全 CTest/native 矩阵。
- `nm -C` 实际检查两配置 primitive 均无 `TableComponentHost`/SDL 符号；重新 configure 后 Core include/link 正负 guard 与 catalog **3/3**。clang-format 22.1.3 **1038 文件、0 失败**；doctor healthy、strict **107/107**、diff check 通过。
- 进度保持 **74/135**；100 samples / 458 frozen IDs / 五份 FULL 66 域 golden 不变。只读准备发现的 Table Override aggregate 前缀兼容修正留在 T，不混入本提交。
- 来源 [原记录](retained-translation-phase-linux.md) 仍标来源。raw logs/命令/指纹/二进制与源码副本仅 ignored out；此阶段未新增 native/GPU、Windows/MSVC/D3D12、物理 IME/AT 或实际跨显示器证据。独立 main SHA 由本文件首次加入的 Git 历史和交付回执确定。
