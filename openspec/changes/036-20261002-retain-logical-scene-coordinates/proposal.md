# Proposal

## Why

035 已分离 host、renderer 与上传事务，但 Quad/Glyph 的 Core store 仍保存 NDC，组件自行按 viewport 打包，CPU 数据与 GPU ABI 共用类型。这使后续 backend 必须继承现有坐标约定，也让 resize 的重打包责任散落在组件中；本 change 先补齐 logical scene 边界。

## What Changes

- **BREAKING**：内部 Quad/Glyph CPU scene 合同升级为 logical scene v2，保留 logical 长度、正向尺寸、平移和裁剪；CPU 类型不再作为 GPU 字节布局。
- 将 Quad GPU 资源与 Quad/Glyph packed 类型、坐标转换放入 `renderer/common`，所有 renderer 消费同一 packed GPU ABI v1。
- 由显式 device metrics 驱动打包；metrics 变化触发完整重新打包，普通 dirty update 保留范围上传，失败沿用 035 的事务重试与 epoch 恢复。
- 迁移 Button、Selection、Divider、Text 装饰、Gallery reference surface 和 minimal 路径，移除 Core 的 Quad GPU 同步入口。
- 更新架构合同与测试，在 Windows 完成平台通用逻辑验收与真实 SDL GPU 验收，单列 Linux 原生验收。

不新增 WebGPU、WebGL、移动端 backend，不改变公开组件 API、字体栅格 phase、shader 或现有视觉设计。

## Capabilities

### New Capabilities

- `logical-scene-packing`：定义 logical CPU scene、renderer 的 GPU 打包、metrics 失效及与事务恢复的组合合同。

### Modified Capabilities

无；当前 `openspec/specs/` 尚无已同步的 capability。

## Impact

影响 `graphics`、相关组件、`renderer/common`、SDL/Recording 的 packed 类型消费、examples 和 tests；不增加第三方依赖。风险集中在尺寸正负号、圆角归一化、glyph 物理像素对齐和 metrics 改变后的 stale bytes，通过独立数学断言、Recording 字节检查与真实窗口覆盖。
