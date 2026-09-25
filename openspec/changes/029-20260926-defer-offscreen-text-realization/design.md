# Design

## Context

文本宿主先调用 LayoutEngine，再逐个遍历 mounted text；已有 clip 判断在 `needs_layout` 为 false 时跳过离屏记录。首次布局时 bounds 已由 layout 更新，但该条件禁止跳过，因此长文档的非可见文本进入 `TextSceneService::synchronize`，产生 glyph instance、raster 和 atlas 上传。028 首帧有 1,077 个 glyph 纹理区域上传；这些区域是否均来自离屏文本需由本 change 的实测确认。

## Goals / Non-Goals

**Goals:** 首次或重新布局后使用最终 bounds 做离屏判断；离屏文本的最新状态在重新可见时实现化；保持测量、布局、滚动相位与 fragment 顺序合同。通过平台通用测试及 Windows D3D12 窗口验收。

**Non-Goals:** 不跳过 measure，不引入空间索引、虚拟列表、后台字体任务或新的 atlas 淘汰策略；不声称 CPU 首帧下降等于 GPU 时间下降。

## Decisions

1. **在布局后判断。** `layout_and_synchronize` 完成 layout 后读取每个文本 node 的最新 bounds、translation 和 clip。bounds 为零或与带现有 32 logical px guard 的 clip 不相交时，跳过本轮绘制同步。这个 guard 沿用现有保守范围，暂不收紧 ink bounds。
2. **按可见性重新进入。** 文本仍保留 mounted record 与待处理的 `TextSceneService` 状态；每帧的文本遍历在记录进入 clip 后执行 `synchronize`，以当前内容和 phase 建立 primitive。离屏内容变化不清除其最新 revision，已存在的旧 primitive 仅在 node 离屏时保留，不能在重新可见时越过同步。
3. **回归与测时。** 构造两个具有确定 bounds 的文本，第二个在首次布局后离屏；验证它已测量但没有 glyph primitive，移动进入视口后生成 glyph；离屏内容更新再进入时比较最新内容与 geometry。Windows MSVC Debug 定向测试后以 Release 干净构建运行五个独立 D3D12 `--scroll-acceptance` 进程，比较 028 首帧 CPU、纹理上传、240 步滚动终态与输入验收。

## Risks / Trade-offs

- 已实现化的文本离屏后可以暂留 glyph instance；本 change 优先避免首次生成，不回收已创建资源。
- 进入视口的首帧会承担该文本的 raster/upload 成本；持续滚动与显示结果须验收，不能只比较首帧。
- 无法证明精确 glyph ink 完全离屏时沿用现有 32 logical px 扩张，以保守正确性优先。

## Migration Plan

在 `windows-msvc` Ninja Multi-Config / MSVC 上分阶段提交规划、回归与实现、真实窗口复测、集成状态。OpenSpec CLI 1.4.1 无 `doctor` 子命令；每阶段提交使用既有英文规范前缀。Linux 真窗口结果不由 Windows 代替；本次变更无 Linux 专属实现，平台通用逻辑只需一平台验收。
