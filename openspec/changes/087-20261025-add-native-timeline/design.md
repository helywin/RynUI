# Design

## Context

基线 bfbb492，锁定参考 out/upstream-ant-design-6.6.5，commit 4a39f54842eade4e565ab336ef6097cd7e723cdd。已读 Timeline.tsx/useItems.tsx/style/index.ts：placement 按源索引计算后 reverse；title 触发双侧布局，loading 对应 process 与虚线。现有 Descriptions 提供 passive region clip、语义文本继承与 retained surface；Table 提供 interaction reorder_after；Theme 已有 33 个 token IIFE、heap descendant preflight/self guard 与 Binding bridge。

## Goals / Non-Goals

目标见 proposal。首版只公开可工作的 vertical orientation，不提供不可用 horizontal 参数；loading 为静态 loading 图标，无动画请求。动态 topology、titleSpan、RTL、系统 IME/AT/GPU/Linux/native 验收不在本 change。富 slot 内的控件保持自身行为与 Theme。

## Decisions

- 新建独立 host/model；不复用 Steps 导航 wrapper。固定 key 唯一且非空；无效 enum 在发布状态前拒绝。placement 默认按源索引计算，reverse 后只改可见顺序和实际 interaction declaration order。
- title/content/icon 分别建立 retained passive clip region。measure 分配有限宽度、place 保留子控件自然布局并由 region 裁剪；clip_decorations=false 允许 focus 外扩，外层 root hardclip 仍生效。极窄与零高不通过缩小真实 Button/Input 来规避溢出。
- root 与每 item 保留身份；hidden 分支 out_of_flow，空/全隐藏 root 不占 flow gap。强制 boundary suppression 使用现有 window services；每次调用可能触发用户回调后按完整 ComponentId generation 回取，清理自己注册的 input block。
- 独立 Timeline colors/metrics、config/override、inherit/inherit=false、component seed algorithm、snapshot hash/equality/JSON、append-only identities 和精确 dirty phase。新 token snapshot 用 shared_ptr；只追加 IIFE，不调整旧 33 块原文及顺序。
- Theme 最终派生统一验证 finite/nonnegative/组合 extent 与 font>=.5 且可渲染；依靠现有 descendant preflight 保证整个树更新原子性。保留现有 Time 默认配置与 switch publish_theme helper，不改 PE 栈。
- 所有图元走 logical CPU scene 和 retained surfaces，无新依赖/renderer 特例；idle 不请求连续帧。

## Risks / Trade-offs

- 极窄时自然 glyph/控件超过分配宽度 → 以真实 atlas ink、surface 和 hit clip 验证，focus 使用 decoration clip 单独验证。
- 反序与隐藏同时改变导航 → 验证真实 Button/Input/Select Tab 顺序、草稿、preedit/capture/session 和 popup 不复活。
- Theme 全链回归 → 新构建生成五份 goldens，递归比较旧 typed 字段完全一致、仅新增 timeline/identity，保留旧 ordinal。

## Migration Plan

### Astra R1 必要修正

在 4545748c 的纯后代修复原有 finite clip/loading 合同，不新增功能：普通 document 的 fill 转换只准入 Timeline host 提供的 retained 子树，沿已有 clipped-fill 路径处理，并分别使用 content/decoration clip。原 normal root 保持原始绘制，window-layer 准入保持原谓词；失活或关闭转换开关时清理 surface/content 两类旧转换缓存并恢复生产者 effects。

正序 rail 取下一显示项的 loading，反序取当前项。tail 使用独立动态 range/fragment，最多每 range 4096 quads；缩小、零宽、隐藏及 teardown 释放多余 ranges。可见 dash 数量在循环前用 double 计算，保留原轨迹相位，以整数计数迭代；超出现有 logical scene 索引可表示数量时显式拒绝，不扩大 renderer/ABI 上限，不静默截断。

纯新增 API；本隔离 worktree 分阶段本地提交，父负责集成与独立评审。平台通用验收使用 Windows MSVC windows-msvc-headless、Ninja Multi-Config Debug/Release、最多 4 jobs。只运行 Timeline 与一次去重影响组合，不重复历史全套/长 Gallery。回退通过撤销本 change 提交。
