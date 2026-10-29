# Design

## Context

见 proposal。当前 `GlyphInstanceStore::replace` 在判断等长前分配全 store replacement；零长度插入已有单独 alias-safe 分支。100 已把 GPU buffer 改成倍增，新增后缀上传及失败重试需要保留。

## Goals / Non-Goals

**Goals:** 等长更新的复制/临时空间与局部输入长度相关，不与背景总量相关；GPU 增长在 backend 能力允许范围内保持摊销成本。

**Non-Goals:** 不建立新 glyph allocator、不改 packed GPU ABI、不移动 renderer 边界、不重做 Input culling。

## Decisions

1. 在全 store replacement 之前处理等长。先验证 range 和尺寸，再为 dirty 元数据准备可能抛异常的空间；对重叠输入使用安全复制策略，只有成功后才提交局部数据和 dirty。保留现有变长路径，避免把小修扩为存储重构。
2. 复用 100 的 live count/capacity 分离。100 独立审查指出最大 buffer 288B 时 2→3 glyph 被倍增为 384B；该小修由父线程在 100 内完成，101 复用已验证实现并补持续追加基准，不并行改相同 GPU 文件。
3. 测量固定 4 glyph 更新、背景 1k/10k/100k；分别覆盖外部输入、相同及重叠 store span。记录分配/复制字节、dirty 范围与时间分布。不能仅使用计时波动认定修复有效。
4. GPU 测量稳定背景后逐个追加 1,000 glyph、容量内删除/恢复、能力边界、上传失败和 epoch 改变。记录实际分配和上传计数；正式 MSVC/Ninja Multi-Config HEADLESS Debug/Release 运行，计时三轮无并发构建。

## Risks / Trade-offs

- 自引用源被覆盖 → 覆盖前确认重叠并使用允许重叠的等长复制，测试左右重叠及自身替换。
- dirty 分配失败发生在数据写入后 → 抛异常的准备必须先于不可回滚数据写入。
- 容量优化越过设备上限 → 使用同一容量计划约束 preflight 与真实分配，并检验设备 epoch 后的新能力。
- 新 benchmark 干扰用户工作 → worker 只交源码；父线程集中运行短 HEADLESS/微基准，不并发编译或开 GUI。

## Migration Plan

按任务分两段验证后提交；无公开 API 迁移。任一回归可独立回退当前阶段；历史 099/100 已验场景不无故重复。独立审查与 main 集成完成前不宣布 all_done。
