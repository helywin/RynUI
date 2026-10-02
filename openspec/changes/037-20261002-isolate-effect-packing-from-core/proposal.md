# Proposal

## Why

036 已将 Quad/Glyph GPU 打包移入 renderer/common，但 `rynui_graphics` 仍编译 RoundedEffect 的 shader ABI、NDC/pixel 转换与 shader reference，SceneDeviceMetrics 也只是 Effect 类型的别名。现有构建守卫允许 Core include/link renderer/common，尚不能约束后续组件只发布 logical scene。

## What Changes

- 把 RoundedEffect packed 类型、打包和 GPU reference 迁入 renderer/common，Core 保留 logical geometry、store、culling 和 CPU coverage reference。
- 为 Quad/Glyph/Effect 建立独立 `SceneDeviceMetrics`、统一校验与 logical viewport 转换，拒绝无法表示的 viewport；维持 packed GPU ABI v1 和既有有效 metrics 的数据。
- Effect 同步失败失效成功 metrics 缓存；abandon 清空旧代际容量与 metrics，保证完整重试和新 buffer 创建。
- 强化 include/link 守卫，Core 不依赖任何 renderer，renderer/common 可依赖 Core，继续禁止具体 backend/OS 依赖。
- **BREAKING（内部接口）**：移除 `graphics/rounded_effect_gpu.*` 与 graphics namespace 的 GPU 类型/metrics；迁移所有内部 consumers 与测试。公开 `ryn` 组件接口保持。
- 更新正式边界、验收证据与英文 Conventional Commit 规则；规划完成后按用户授权直接进入 apply。

## Capabilities

### New Capabilities

- `renderer-scene-boundary`：三类 primitive 的 GPU packing 和 device metrics 由 renderer 拥有，Core 依赖方向受构建合同约束，Effect 资源失效后可完整重建。

### Modified Capabilities

无。主 specs 尚未同步；035/036 的平台通用和分平台证据继续保持各自状态。

## Impact

影响 `src/graphics/rounded_effect_gpu.*`、renderer/common/SDL/Recording consumers、CMake target/guard、packing/resources tests 与相关文档。无新依赖、shader 改动或新 OS/backend；不实现浏览器、移动端、自动 device-loss 检测、异步资产或零尺寸 surface 策略。风险为 ABI/namespace 迁移漏项、缓存失效后的旧 buffer 误用和 guard 误伤，分别用 literal/reference、失败/重建及正负配置 fixture 验证。通用测试在本机 Windows/MSVC 完成一次，Windows/Linux 原生验收独立记录。
