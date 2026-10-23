# Design

## Context

动机见 proposal.md。现有 Tabs 使用 set_branch_active 保留正文，Card 提供 retained header 区域布局；WindowComponentServices 统一处理 logical owner、Modal/Spin 输入限制和隐藏分支清理。Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 Collapse style 已按 sources.lock.yaml 的 SHA256 `b91b3252b6f82024457c540f0926f8b86e38f0ddf62756cd18816ad2dc4e93ee` 核对。

## Goals / Non-Goals

**Goals:** 固定 uniquely keyed panels、全部 slot eager mount 一次、直接开合和真实编辑器保留。新组件仅使用 logical CPU scene 和共同 surface/interaction/icon。

**Non-Goals:** 不改 Gallery、README、组件完成表或 support overlay；不引入 pending Table/Menu/InputNumber；不增加新 renderer 或公共通用容器抽象。

## Decisions

- API 使用 CollapseProps、CollapsePanel、CollapseTitle/CollapseBody/CollapseExtra；activeKeys 是 `Prop<std::vector<String>>`，defaultActiveKeys 为 mount-only，accordion/size/disabled 可响应。key 集合按输入顺序归一化，accordion 留首个已知 key，用户 toggle 输出规范集合。相比动态列表，固定拓扑使生命周期与成本确定。
- 每个 panel 保留 header、title、arrow、extra、body。根自定义 measure/place 只测可见正文并更新 sibling 流布局；隐藏 branch active 交给共同机制，不自行复制输入会话/浮层清理逻辑。所有 header 是正常 Tab stop；方向导航只移动焦点。
- header 仅 target handler 切换，extra 原生交互独立；不对 header 子树整体 disabled，避免误禁用 extra。只在共同 interaction eligibility 和 component_active 均允许时响应。所有外部回调前复制值，后续访问重新校验 owner；branch 切换回调重入同样重新查找 state。
- 复用 Icon 的响应式 name 显示 Right/Down，不动画。bordered 首版采用圆角根背景/outline、header/body fill、分隔线及 focus outline，不扩展 borderless/ghost aliases。
- 新增独立 CollapseThemeToken 分组与 typed overrides，按现有 ThemeScope subscription 接入。保留完整现有 snapshot 成员、serialization、hash 和订阅；只追加 Collapse 数据与必要黄金结果。
- Windows 在本 worktree 的 `out/build/windows-msvc-headless` 使用正式 preset、Ninja Multi-Config、MSVC，最多 4 编译 jobs；不使用根目录 build。平台通用 focused contracts 在 Windows Debug/Release 实跑一次；不创建重复 Linux 或 GPU/manual IME 验收任务。

## Risks / Trade-offs

- [隐藏正文触发 onBlur 销毁 owner] → 每次可能回调后重新校验 ComponentId，测试销毁和同步受控 echo。
- [多开高度变化与 retained 缓存] → 真实 Flex sibling reflow 测试，不仅检查 branch flag。
- [Theme 扩展漏掉既有字段] → 运行现有 Theme/Modal/Spin/Tabs focused regressions，验证继承、identity、goldens 与 material locality。
- [eager mount 成本] → 首版只面向固定数量面板；不声称 lazy、virtualized 或动态 reconciliation。
