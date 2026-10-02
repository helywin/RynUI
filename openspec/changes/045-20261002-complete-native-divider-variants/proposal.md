# Proposal

## Why

用户要求把已部分实现的组件补齐原生桌面功能。034 的 Divider 覆盖基础线条，但再次核对 Ant Design 6.6.5 源码发现还缺 dotted、尺寸、逻辑标题方位和显式长度间距，不能仅因 Web API 不移植而把目录改成 implemented。

## What Changes

- 增加 typed/reactive `DividerVariant`（Solid/Dashed/Dotted）与 Small/Middle/Large 尺寸，保留原有 `dashed` 与默认几何。
- 增加标题 Start/End 与原生 LTR/RTL 方位，Left/Right 保持物理含义；显式逻辑长度间距与现有 Theme/None/Ratio 并存。
- 圆点使用共同 logical RoundedEffect，保持透明间隙、节点 translation/window clip、资源上限与销毁合同。
- Theme token/override/identity/诊断、公开说明、Gallery 样例/支持目录与收尾清单同步更新。
- 平台通用 headless 合同和 Windows 原生 GPU/字体/DPI/resize 验收独立提交；Linux 原生项独立记录。

## Capabilities

### New Capabilities

- `divider`：在 034 既有合同基础上补齐原生线条变体、尺寸、逻辑标题方位与长度间距。当前 main specs 清单为空，复用已有 change 的 capability 路径。

### Modified Capabilities

无。

## Impact

影响 Divider public/runtime、Theme、共同 retained surface effects、测试与 Gallery；无需新增第三方依赖、GPU ABI 或 backend 分支。非目标为 DOM refs、CSS styles/classNames、HTML 属性、Web 兼容别名及全局文字 bidi 排版；此次方向只控制 Divider 标题相对于线条的位置。风险是旧比例间距/default margin 兼容、极小线宽导致过多 primitives，以及 dotted 裁剪和圆点形状；以旧 API 回归、4096 上限、renderer reference 与实际 GPU 图像验证。

来源（2026-10-02 核对）：[Divider API](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/divider/index.tsx)、[Divider style](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/divider/style/index.ts)。规划完成不表示功能实现；按用户“写完 change 就开始改代码”的授权完成规划后立即进入 apply。
