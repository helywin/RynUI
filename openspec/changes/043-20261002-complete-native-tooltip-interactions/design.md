# Design

## Context

见 proposal 的动机与 040 `tooltip-overlays` 合同。现有 Tooltip 的 popup 是独立 window layer，String Text 保留；PointerRouter 只有命中路径，窗口空白没有 handler。箭头由 32 条很薄的 quad 模拟，GPU 抗锯齿造成灰色。已有私有 CFF outline 容器经 FreeType、R8 atlas 和共用 glyph scene 呈现 Icon。

锁定参考：Ant Design 6.6.5 [Tooltip](https://github.com/ant-design/ant-design/blob/6.6.5/components/tooltip/index.tsx)、[placements](https://github.com/ant-design/ant-design/blob/6.6.5/components/_util/placements.ts)。其 typed title 接受内容，pointAtCenter 改变角位置锚点，角箭头固定内距。只参考设计合同。

## Goals / Non-Goals

**Goals:** 保留普通内容身份、受控值及点击顺序；窗口范围的非消耗观察；箭头资源遵守 owner/epoch、atlas 上传与 logical 坐标。

**Non-Goals:** 不增加 Popover 复杂操作容器、不移植 DOM 容器及 React 专属选项，不新增 backend 或渲染 ABI。颜色继续由 typed Theme/token 配置。

## Decisions

### 标题与动作 API

新增 `TooltipTitle` typed slot overload，与显式 `.title()` 互斥；`.titleAvailable(Prop<bool>)` 用于声明富内容是否可显示，默认 true。富标题只在 mount 执行，继承 foreground/typography，reactive Text 通过既有 dirty/layout generation 重新测量浮层。拒绝以 callback 每次 show 重建内容。

旧 `TooltipTriggerMode` 保留所有值，末尾新增 Click/ContextMenu。组合用 `TooltipTriggers{hover,focus,click,context_menu}`，`.triggers(Prop<TooltipTriggers>)` 与显式 `.trigger()` 冲突。动作锁存与 hover/focus desire 共用 open 请求；点击关闭/Escape 清除锁存并抑制仍停留的 hover/focus，离开后解除。受控请求使用上次请求或实际 open 切换，未回写也能撤销。右键记录 logical 指针点，位置不跟随之后的 hover move。

### 共用 pointer 观察

PointerRouter 安装 owner-thread 的单一 post-route observer，参数为输入、实际 hit 和 primary press origin；路由后、up 清除状态之前通知，即使无命中也通知。使用 callback 副本，保持重入守卫和异常 abort。hover/focus 回调销毁 hit 后重新验证目标，避免 require stale。WindowComponentServices 将事件传给参与者；Tooltip 快照已挂载 IDs 后逐项查询，不改变事件传播。popup 内点击属于内部，外部 down 关闭动作提示；不将普通 hover popup 变为可交互 Popover。

替代方案为全窗口 hit region 或 pre-route observer：前者改变命中层级与 child 行为，后者可在 Button 激活前销毁 target；选择 post-route 保持激活顺序。

### 箭头定位与资源

角位置默认箭头中心距 popup 边缘为有限 token 半宽/圆角内距，pointAtCenter 使用 anchor 中心加对应偏移；中心位置和 overflow 移位时 clamp 箭头到合法内距。12 placements 仍主轴 flip，再 viewport shift。

私有 CFF 容器加入 RynUI 自有四向三角形 primitives，使用独立私有码位，不扩展公共 IconName 或改变已有 Ant glyph 索引。箭头挂载为独立非布局 window layer 的 retained Text glyph，foreground=Tooltip background，font size/line height 来自 arrow token；相同资源与其他 glyph 一起提交。四向字形填充连续覆盖，替代 quad strips，生成器与 manifest 记录边界/哈希。无需新增 raster库或 GPU path。

### 验证与迁移

Ninja Multi-Config，Windows MSVC。阶段 2/3 focused headless Debug/Release，阶段 4 完整 headless suite、边界守卫与格式/OpenSpec；阶段 5 native Windows Debug/Release build、受影响 CTest、D3D12/DXIL system/1/1.25/1.5/2 × 两配置；Linux GCC/Clang Vulkan/Fontconfig/Wayland 单独 pending。旧 String Tooltip/Slider hints 测试保持兼容；每个验证阶段独立提交。

## Risks / Trade-offs

- 回调移除组件/target → generation ID 复核、mounted 快照、observer 副本和异常恢复测试。
- title slot 更新导致浮层 size stale → 开启时测量、layout generation 与 dirty 源验证，不重跑 Content。
- 新 glyph 导致资源哈希变化 → deterministic 生成、许可/锁定 source 验证及 atlas 合同测试，实际 GPU 回读检查四向填充。
- 一个 build 目录并发配置损坏增量缓存 → 同目录 Debug/Release 顺序构建，最终图像验收后不再改变 EXE。
- Linux 本机不可用 → 保持独立 checkbox；继续用户授权的其他原生组件收尾。
