# Design

## Context

见 proposal.md。现有 WindowComponentServices 统一拥有输入与 retained scene，LayoutEngine 支持组件测量/放置，Typography 已有单样式排版与编辑。通用与布局目录的 Web 专属接口不迁入公开 API。

## Decisions

1. 按通用、布局顺序交付。FloatButton 复用现有 Button、Tooltip、Badge 和窗口层能力；Group 复用受控弹层输入边界。BackTop 用显式 typed 滚动目标及回调，进度/动画借助宿主单调时钟与 reduced motion。拒绝组件内 SDL 分支。
2. 富文本使用 typed runs 表达语义，不接受任意 HTML。整段来源统一处理 Unicode 边界和双向信息，样式分段使用实际字体与装饰；HTML 只用于输出。剪贴板接口提供默认纯文本回退，SDL 适配发布两种 MIME，数据所有权保持到平台释放。
3. 新布局采用 retained ComponentLayout，测量与放置共用一代计算结果；属性通过 Prop 连接失效队列。响应断点共享窗口可用宽度，默认 xs=480、sm=576、md=768、lg=992、xl=1200、xxl=1600、xxxl=1920，匹配长期架构；不引入 DOM 或百分比 CSS 字符串。
4. Row/Col 配置支持逐字段响应覆盖。Layout 自动检测直属侧栏，背景与尺寸来自 Theme/Component Token。Masonry 使用稳定键和实际高度选择最短列，指定列优先；重新布局不重挂载内容。
5. Splitter 统一求解尺寸及相邻增量，最小/最大值在逻辑长度或百分比解析后限制。捕获由已有 PointerRouter 管理，键盘与指针共用调整入口。lazy 保留预览几何直到提交；隐藏销毁通过 owner/generation 管理，防止取消后的回调访问旧状态。
6. 每阶段只运行相关测试。平台通用合同使用 Linux GCC HEADLESS Debug/Release（Ninja Multi-Config）；构建原生 Gallery 供用户自行运行。Linux 自动窗口仅在 tools/run_background_wayland.py 私有 compositor 中执行。Windows MSVC HEADLESS 与原生剪贴板/窗口验证独立记录，不以 Linux 代替。

## Risks / Trade-offs

- 富文本字簇跨样式边界 → 保留完整来源和 Unicode 边界，针对 CJK、组合字符、RTL、省略添加合同测试。
- 布局期间回调修改或销毁内容 → 回调在安全同步点执行并重新验证 owner/generation。
- 小窗口导致约束不可满足 → 保证非负、有限、总尺寸确定，显式记录约束处理顺序。
- 错把目录标成完成 → 仅实现和对应验证完成后更新 support-overlay；平台覆盖单独记录。

## Migration Plan

新增 API 不破坏现有调用；General 与 Layout 分阶段独立提交并推送。新增示例接入各自 canonical 页，更新生成目录。自动测试日志和运行产物只存 out/，提交精简结果。人工体验为可选建议，不列任务或完成门槛。
