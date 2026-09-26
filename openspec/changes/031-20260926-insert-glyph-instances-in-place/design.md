# Design

## Context

`GlyphScene::replace_text` 先把新文本 glyph 构造成独立 `PendingGlyphText::instances`，再调用 `GlyphInstanceStore::replace`。新可见文本此前的 range count 为 0；当前实现为每次替换分配 `replacement_size` 大小的 vector，复制整段前缀、新内容和后缀，再 swap。030 后第 240 步跳转仅新增 55 次 raster，但单次诊断的 `replace_text` 共约 11.5 ms。正式五进程最长帧中位数 15,227 µs。

## Goals / Non-Goals

**Goals:** 零长度 range 插入复用 vector 的几何扩容，避免每个新文本都全量分配和复制；保持 instance 顺序、range、上传 dirty 与重叠源数据结果；用正式 D3D12 复测最大帧和首帧。

**Non-Goals:** 不改变非零长度替换的旧事务实现，不引入稳定 span、分页 arena 或 GPU 间接寻址；不把 CPU 提交时间称为 GPU 执行时间。

## Decisions

1. **受限快路径。** `range.count == 0 && !instances.empty()` 时，在长度与范围检查后调用当前 `instances_` 的 `insert`。`GlyphInstance` 只含可无异常复制/移动的定长数值字段；vector 在容量足够时移位尾部，在不足时按标准 vector 扩容。没有新内容时维持现有空操作路径。
2. **源重叠与失败。** 用指针全序比较判断源 span 是否与 store 内存范围重叠；重叠时先复制源 glyph 到小的临时 vector，再插入，避免原位移位或扩容破坏输入。失败可在临时复制或 vector 分配阶段传播，成功前不修改 dirty ranges。长度超出 `uint32_t` 时仍抛出原错误。非零长度替换继续使用已有完整事务路径。
3. **脏范围与验证。** 插入后沿用现有 `material_dirty_ranges_.discard_shifted`、`geometry_dirty_ranges_.discard_shifted` 和 `[range.first, new_size)` geometry 脏区合同。单元测试比较多次中间插入的确切顺序/范围，检查与 store 重叠的源及百次小插入时底层 data 指针变化次数明显少于插入次数；后者只验证容量复用机制，不替代真实 CPU 测量。
4. **性能判定。** 正式 `windows-msvc` / Ninja Multi-Config / MSVC Release `--clean-first` 五进程 D3D12 `--scroll-acceptance`，旧基线直接复用 030；报告首帧、滚动平均/p95/最长帧、glyph/texture 上传和最终状态。只有五次最长帧区间明确下降且无材料正确性回归时才声称修复长帧。

## Risks / Trade-offs

- 原位插入仍会搬移该位置之后的实例，复杂度与后缀大小相关；本 change 去掉每次完整数组分配和前缀复制，不声称实现稳定 span。
- vector 扩容仍可造成偶发较大拷贝；其次数应随容量几何增长，而不是随文本数线性增长。
- Dirty suffix 仍可能较大，GPU 上传与 buffer transfer 需要单独测量；仅 CPU 速度改善不足以证明整体收益。

## Migration Plan

规划、平台通用实现、Windows 真实窗口复测、集成结果分别提交，使用既有英文规范前缀。当前 OpenSpec CLI 不支持 `doctor`；全仓已有 6 个独立 strict 失败项。Linux 的平台通用逻辑无需重复执行，Windows GPU 结果不代表 Linux 窗口验收。
