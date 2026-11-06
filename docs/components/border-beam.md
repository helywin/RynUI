# BorderBeam

原生合同：`BorderBeam(BorderBeamProps, BorderBeamContent)`。Windows/MSVC Debug/Release 定向测试和真实 D3D12 Gallery 验证已完成，结果见[验收摘要](../../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/decoration-windows.md)。

`duration` 是单次完整圆角周长的运动周期，必须为正；`direction` 支持 Clockwise / Counterclockwise。颜色、线宽、圆角和光束长度来自 `ThemeConfig.border_beam.tokens`。真实子内容保留身份与原生命中，外部尺寸只通过 `LayoutStyle` 控制。

实现复用既有 logical `RoundedDashPath`：一个 dash 加一个 gap 覆盖完整圆角中心线周长，phase 随单调时钟前进，因此有一个实际沿边框和拐角移动的线段。光束长限制在周长的 75% 内，保持可见 gap；此原生效果采用单色线段，不声明渐变尾迹。无须 Progress continuous paint、GPU ABI、shader 或组件私有上传扩展。

`visible=false`、禁用、祖先离页、窗口失活和 reduced motion 停止动画 deadline；窗口失活与减少动态效果保留静态光束。`ThemeConfig.seed.motion=false` 同样停动。恢复时从保留 phase 继续，不补跑暂停时间；静态内容与属性更新不重建子内容。

Gallery 可调整时长、方向、颜色/圆角，切换减少动态效果及显示状态。原生自动验收使用固定时钟推进 200ms 捕获局部运动帧，检查 path phase 变化、reduced motion 停帧及内容身份；采用 hidden GPU readback。Linux、其他实际 DPI、系统 IME 和 AT 未实测；原始输出仅放 `out/`。
