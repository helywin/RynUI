# Design

## Context

见 proposal 的动机。当前 GlyphGpuResources 用 backend alignment 创建临时 vector；SDL 消费 padding 后的 bytes，而 Recording 把同一个 transfer_offset 解释为源偏移。SceneBackend 有 owner/epoch/事务与 attachment 校验，但没有必需视觉能力或输入限制合同。037 的 Core 守卫、logical scene v2、packed ABI v1 继续有效。

## Goals / Non-Goals

**Goals:** 集中收口当前 backend 适配接口，下一轮组件只使用 logical scene；保持 SDL batch/chunk、独立 upload、shader 和细粒度 dirty 行为，给未来 backend 明确源 bytes、能力与恢复合同。

**Non-Goals:** 新 OS/真实 GPU backend、通用 RHI、异步资源 provider/fence、自动 device lost、能力不足时自动 fallback、改变组件视觉或字体 rasterization。

## Decisions

### 源与 transfer 分离

在 renderer/common 定义 GlyphTextureUpload：page、目标 rectangle、size_t source_offset、uint32 source_row_pitch、span<const byte> bytes。源 offset 相对于 bytes 起点，与目标 x/y 独立。统一 validator 用减法边界检查，允许末行只有有效 pixels；先验证目标 uint64 extent，再验证 offset/stride/span。GlyphGpuResources 直接借用 atlas.page_bytes 与 dirty plan，无逐区域 staging。

保留 padded payload + capability 的备选仍迫使 common 选择具体 backend layout，因此移除。保持 R8-only 合同，不引入未实现的 Image/array/多格式系统。

### 后端复制策略

SDL 独立的、无 SDK 依赖的 transfer layout helper 检查 256-byte row pitch 和 uint32 byte count；将源逐行复制至已映射的 transfer buffer，填零 row padding。batch 继续使用 TextureUploadBatchLayout 的 512-byte source alignment、已有 chunk/oversize 策略；独立 upload 使用 offset 0。避免重新引入临时逐区域 vector。

Recording 在接受时仅复制 width×height 的有效 pixels 到紧凑 owned vector，pending 内 offset=0、pitch=width。finish/cancel 不再接触 caller memory。测试双也按照源视图记录 pixels 或有效字节计数，不能把完整 page size 算作上传量。

### 必需能力与输入限制

新增 renderer/common SceneBackendCapabilities：版本、三类 primitive、R8、ordered draws、partial uploads，以及非零 maximum_buffer_bytes/texture width/height。baseline factory 明确提供当前合同；SceneBackend 的 capabilities 是纯虚方法，backend 必须显式报告。

SceneResources 构造前先验证能力，再创建低层资源。每次同步在 begin 前验证限制：Quad 与资源同步共用 `max(required, old_capacity×2)` 的容量 helper（初次为精确 count），epoch 改变不计旧容量；live Effect 按既有 power-of-two growth 保守估算（高于 2^31 时使用精确 count），Glyph 使用精确 count。Effect 的 preflight 不为了计算限制而修改 compact/store；live count 估算可能比实际可见 GPU count 大，这是明确的输入预算。已有资源容量由先前相同限制下的创建保证。特别测试 Quad 从非二次幂容量增长时，preflight 不低估实际申请。

SDL 从实际 device 查询 R8 sampling，其他能力由现有 pipelines/API 保证；buffer limit 采用 SDL uint32 接口上限，texture extent 上限只约束可表示输入，不声称 SDL 暴露了完整硬件限制。实际 create 仍可能失败并走共同恢复。Recording 构造参数提供 capabilities fixture，保持 owner/epoch/handle 校验。

不添加组件 capability 分支或 silent fallback。所有版本/能力拒绝应给出具体字段原因，避免只返回笼统 failed。

## Risks / Trade-offs

- [源整数与短末行] → literal offset/stride fixtures、越界/overflow、目标未改写及 failure injection 顺序测试。
- [mapped padding/chunk] → 在 SDK 无关 helper 上检查 literal padding、256/512 对齐、chunk 边界/oversize；Windows 原生实际 D3D12 上传和窗口截图独立留证。
- [能力误报或过度承诺] → 强制 backend 显式 manifest，逐字段负例；SDL R8 查询，软件输入上限与硬件可用内存明确区分。
- [失败清 dirty、epoch 丢像素] → Recording 多区域 owned bytes/cancel/commit retry 与真实组件 scene/CPU atlas 逐字节比较。
- [性能退化] → 不改变 transfer 合并策略，移除 common staging；继续局部/idle/no-allocation 回归。不把 CPU 数据或 submit 时间描述为 GPU 时间。

## Migration Plan

规划单独提交后，连贯迁移 source API、两后端、能力 preflight 和全部 consumers；运行 Windows/MSVC `windows-msvc-headless` Debug/Release 完整 CTest，记录平台通用证据后提交。使用 Ninja Multi-Config 构建 native Debug/Release，分别留 Windows/GPU 与后续 Linux 原生验收项；不重复要求 Linux 运行平台通用逻辑。回退使用完整阶段 commit，不保留同时存在的旧/新上传 API。此框架 change 完成当前环境验收后，继续独立组件 change。
