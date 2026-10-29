# Design

## Context

参见 proposal。当前 163 个 Input 各同步三个 glyph scene，即使完全裁剪或 opacity=0；普通 Text 已使用既有 synchronize_culled。Gallery 每帧为样例锚点重复扫描 mounted Text。滚动采样清零计时但未清零 reconciliation，启动的一次同步混入结果。

## Goals / Non-Goals

保留所有输入几何、IME/caret、样式和资源生命周期，只在提交字形层时剔除不可见结果。保留实际目录导航和 resize anchor 恢复。不引入虚拟化、异步布局、公共缓存 API 或新 renderer ABI。

## Decisions

- 使用已有 culled 路径，不跳过整个 Input 几何协调；避免陈旧命中、session 或尺寸。
- 布局完成后重建锚点，纯 translation 复用；初始状态仍强制构建。若发现拓扑不经过布局更新，增加对应失效依据而不以每帧全扫描兜底。
- 在采样 reset 同时清零 reconciliation；检查条件保持为零。
- 初轮优化后 Release 平均仍约 13 ms；校正分项发现 Button/Input 几何各约 0.8 ms。祖先状态缓存实验未降低平均成本，已撤回，不增加这项缓存。Button 仍逐帧计算和移动八个完全透明的 loading 分段；只更新有 opacity 的分段，显现时由同帧 geometry 恢复当前位置。性能记录覆盖实际参与者数，移除错误的固定前四名映射。
- 透明分段跳过后上传区间变碎（实验 102447 区间），不能只看字节下降。Quad 资源默认保持精确范围，Gallery 显式选择最多 1 MiB、至多 8 倍脏量的 span 合并；仅打包实际脏数据，合并上传复用有效缓存字节。Glyph buffer 与既有 Quad 策略一致使用倍增容量，避免每次增加少量字形都重建 buffer。GPU ABI、上传事务失败重试及 Core 分层不变。
- Windows MSVC Ninja Multi-Config：headless Debug/Release 定向回归，Release Gallery Recording 代表与原生 D3D12 自动滚动同配置前后比较。只在用户授权下短暂打开窗口，测量时不并发构建。

## Risks / Trade-offs

- 剔除后恢复丢字形 → 检查实际 glyph range、内容更新与身份。
- 失效锚点 → 回归宽度、重排与真实目录操作。
- 批量 range 变动产生额外上传 → 用真实计数和帧时间比较，未改善不声明完成。
- 本机共享负载影响数据 → 同配置、无并发构建，记录边界；不宣称所有高刷设备都达到固定 FPS。
