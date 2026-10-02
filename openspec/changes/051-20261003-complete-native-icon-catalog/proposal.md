# Proposal

## Why

现有 Icon 只有锁定资源包的 14 个具名单色图标，目录仍为 partial；用户要求把已有组件的原生桌面功能补齐。补全官方离线目录、双色、自定义向量与旋转/spin，使应用无需引入网络图标或组件私有上传路径。

## What Changes

- 锁定的 `@ant-design/icons-svg` 4.6.0 全部 848 个 outlined/filled/two-tone 图标生成 typed `IconName` 和逐层 CFF 轮廓；保留原有 14 个枚举数值与 Tooltip 私有箭头 codepoint。
- **BREAKING**：`IconName` 底层类型扩为 uint16_t；共同 logical glyph 增加旋转，packed GPU ABI 升为 v2，由共同 packer、reference 与 SDL shader 消费。
- 提供 reactive 双色配置、rotate/spin 与 typed 原生 Move/Line/Quadratic/Cubic/Close 自定义向量；默认继承 Theme/slot 的颜色与字号，视觉默认值由 Theme 控制。
- 使用既有 TextSceneService / FreeType / glyph atlas 的保留资源，材质或角度更新不重新执行 Component、不重新 shape/rasterize；spin 遵守 motion/reduced-motion、不可见分支与销毁/idle 合同。
- Gallery 覆盖全目录计数、outlined/filled/two-tone、自定义与旋转；通过共同和独立 Windows/Linux 验收后记录证据。

## Capabilities

### New Capabilities

- `typed-offline-icons`：完整离线 typed 图标、双色、自定义向量与 motion 合同；延续未同步到主 specs 的 033 原有行为。
- `retained-glyph-transforms`：保留 glyph 的逻辑旋转、共同 GPU 打包与能力/输入校验。

### Modified Capabilities

无已发布主 spec。`openspec list --specs` 实际返回 No specs found；此前组件合同保留在 changes 内。

## Impact

影响 Icon 公开 API、TextComponent/TextScene/font 维护工具、glyph logical store、renderer/common packing、SDL glyph attributes、HLSL shader/reference、资源锁/manifest、Gallery 和相关测试。无新生产依赖；维护生成器继续锁定 FontTools 4.60.1。Web className/style/React component、iconfont.cn 远程脚本和 DOM API 不移植。

风险是 layer 对齐/副色推导、旋转后的 viewport clip、GPU ABI 不匹配与动画无效唤醒；要求全目录资源重现、色彩与变换合同、身份/缓存/清理回归和实际 GPU 读回。规划文件不代表功能已实现。
