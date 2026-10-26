# Design

## Context

见 proposal.md。基线 `8056de67` 已有 Popover、窗口 notice holder/ref、Timeline 和共同生产目录 anchor。Gallery 通过一个 shared state 保留响应式值，样例追加在文档末尾；当前精确盘点为 358 IDs、291 live、367 Buttons、151 Inputs、1412 interactions。完整无参 frame 上轮耗时 1800.51 秒，不适合每个增量阶段重复。

## Goals / Non-Goals

**Goals:** 将已有 API 组合成真实可操作流程，复用生产目录解析，保留旧样例与输入身份；用有限视口的实际事件和场景检查证明行为。

**Non-Goals:** 不新增或修补 Core/API/Theme，不更改 GPU ABI/renderer/平台宿主；不添加图片、动画、全局服务、异步网络或新的通知持有语义。示例使用显式关闭的持久通知，自动到期合同继续由既有 notice 专项测试覆盖。完整 Gallery 与原生 smoke 放到批次收尾，不把定向结果记成全量或专属平台通过。

## Decisions

- **追加独立样例函数。** 在旧 Skeleton 样例后调用 Popover、notices、Timeline 三个新函数；旧函数正文不改。相比改写已有面板，这使基线保留可直接比对，也避免扩大默认 Windows Debug 栈。新增状态属于 GalleryState；巨大的 ThemeConfig 不在新增局部栈中复制。
- **使用真实 authority 和 holder。** Popover 的 onOpenChange 回写其 Signal，保存反馈来自实际 Input onChange。通知打开时创建 rich description，局部更新只传标题/状态，不重传 slots；关闭回调更新外部按钮资格，关闭后重新打开是新的通知。没有用外部 Text 仿造浮层或提示。
- **外部恢复操作。** Popover 禁用、Timeline 条目隐藏后的恢复按钮放在原内容外。Timeline 备注由真实 Input/Ref 管理，reverse/placement/visible 只改变既有键控布局，不重建 slot。
- **复用生产 anchor。** 扩展 gallery_component_navigation_anchor 的四个 identity 映射，通知的两个目录进入同一操作台。测试通过可见目录按钮取得真实导航请求，再使用共同生产解析和 document viewport 跳转，不能仅按节点平移而绕过入口。
- **聚焦验收。** 增加 `--overlays-timeline-only` 并接入无参 main；一组完整业务流程加 Default/Dark/Compact × 1200/560 有限视口，对实际 glyph/surface/hit 和身份检查。沿用窗口层组件 snapshot 定位自身 region，不用同类型控件的全局下标定位旧样例。不会再次运行所有历史 scoped flags。

## Risks / Trade-offs

- [窗口层与文档滚动混淆] → 使用自己的 owner/region 查找与生产 anchor，检查最终位置和真实命中；窗口层按实际 viewport，普通文档按 document clip。
- [同 key 更新误重建编辑器] → 初次打开才提供 rich slots，更新按钮只传显式需要变更的属性；实际记录 editor/component/ref 代际。
- [通知关闭引发悬空引用或残留命中] → 测试关闭/清空后真实输入消失，外部入口可重开；不在 Gallery 层缓存已销毁的 owner。
- [新样例改变滚动上限和数量] → 旧 IDs/函数体/非目标 overlay 基线比较，实际 inventory 后再更新精确计数。
- [宽窄布局裁剪或焦点装饰丢失] → 固定有限外宽、给原生控件留 margin，检查实际 glyph/fill/hit/外部 clip；不通过放宽断言绕开。

## Migration Plan

本地与 main 使用现有公开 API，无新依赖。按可独立验证阶段提交；父只推 main 并核远端 SHA，不推 t3 分支。采用正式 windows-msvc-debug（MSVC、Ninja Multi-Config，最多 2 jobs）完成一次平台通用 Recording 验证。未引入 OS/GPU 分支，不创建虚假的 Linux 或人工平台完成项。
