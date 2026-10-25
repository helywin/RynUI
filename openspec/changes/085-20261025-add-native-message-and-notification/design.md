# Design

## Context

动机见 proposal.md。已读取 WindowComponentServices、WindowComponentParticipant、ComponentHost、Feedback/Form/Skeleton、输入边界文档及现有组合测试。窗口已有 append_slot、position_window_layers、tick_auxiliary 和 next_auxiliary_deadline；组件身份为完整 index/generation。根 capability inventory 目前为空。

锁定上游为本机 D:/code/RynUI/out/upstream-ant-design-6.6.5，已核对 commit 4a39f54842eade4e565ab336ef6097cd7e723cdd；隔离 worktree 无该 ignored 目录，只读此副本。读取 message/useMessage.tsx、message/style/index.ts、notification/interface.ts、notification/useNotification.tsx 和 notification/style/index.ts。采用 Message 默认 3 秒、顶部 offset 8、内容 padding 9/12；Notification 默认 4.5 秒、宽 384、padding 16/24、四角堆叠、真实关闭控件。上游 pauseOnHover 默认 true；本版明确选择计时不暂停，不暴露该开关。

## Goals / Non-Goals

**Goals:** 在按钮事件内通过绑定的窗口 Ref 动态挂载真实控件，局部更新且保持内容身份，确定性到期与清理。

**Non-Goals:** 不实现全局 singleton、promise、进度条、动画或折叠 stack。loading 是静态 LoadingOutlined；不暴露动画开关。无自动焦点抢占、Escape 关闭或 hover/失活暂停。全局 Gallery、README 与完成表交父集成。

## Decisions

1. 公开 MessageRef / NotificationRef 与 MessageHolder / NotificationHolder。Ref 的 open(key, typed Props, typed Slots)、close(key)、clear() 仅在 holder 存活且代际有效时执行；显式 key 避免额外全局编号。holder 可挂载在任意 window scope，保留逻辑祖先与 Theme。动态内容使用 WindowComponentServices::append_slot，不自行构造独立 Runtime。
2. 共用内部 notice host/model。每个 entry 保留根、字符串 Signals、独立属性连接 scope、typed slot 容器、真实 Button 和逻辑 surface。same-key open 只更新明确提供的字段；未提供 slot 保留，显式 slot 只替换对应 region。普通文本/颜色/placement 更新不重挂其他内容；duration 在更新时按当前值重新计时。
3. maxCount 为每个 holder 的正整数，默认 8；超限移除最早插入 entry，更新不改变顺序。关闭先摘除完整 generation 并销毁，再调用 onClose；clear 和 tick 先快照 IDs，回调新建同 key 不会被旧批次删除。卸载不调用 onClose。所有用户 getter、slot、focus/close 回调后重新回取完整 generation，不持有可悬空 state 引用。
   R1 修正：trim 不固定删除数量；每次回调后直接读取保留的 maxCount Prop authority，再按完整 holder generation 回取当前 entries。这样同一响应式 observer 中尚未提交的上限变化也能立即生效。
4. 使用现有 AnimationTime 和单调 cursor，0 无 deadline，正数只注册最近 deadline；hover/失活继续计时。没有活动计时器时返回空 deadline，不使用 sleep 或每帧 poll。
5. 每个 entry 为正常流外窗口 layer，优先级来自 Theme；保持逻辑祖先，Modal/Spin/隐藏边界自然生效。只在自己的实际 topmost hit 内容范围拦截最终滚轮回退，放在所有内容处理之后；不拦 Escape。open 不聚焦。关闭 focused 内容经窗口服务销毁路径清理 focus/会话，不调用 stale ref。有限窗口约束宽高并 clip_content，越界部分无绘制或交互。
   R1 修正：共同 retained fill 裁剪接纳普通 window layer 的后代；surface 和 content-range 都沿祖先 clip 生成既有 fill effects。window-layer root 仍重置 clip chain，普通正文不进入该路径，装饰使用既有 decoration clip。
6. Message 与 Notification 独立 Theme config 和 tokens，共享小型 typed token 结构。完整接入 derive/override/seed algorithm/validate/merge/runtime identity/serialization。新增 identity 只 append，保留父已有 IIFE 原文/顺序及旧 golden 字段值，维持 heap preflight、self guard、Time 默认配置和 Binding bridge，不扩大 PE 栈。

## Risks / Trade-offs

- [同步回调卸载与重绑] → weak binding + holder/entry 完整 generation；关闭回调最后执行，批次不携带 key 重查。
- [用户 slot 抛异常] → append_slot 的既有事务回滚，恢复有效 entry 索引；仅销毁明确替换 region。
- [超长通知] → 约束到有限视口并裁剪，首版不增加独立滚动容器；富 TextArea 保留其已有滚轮能力。
- [Theme 栈与旧值回归] → 小字段追加、堆上大配置，五份新程序生成的 golden 与旧值深比较。
- [平台证据边界] → Windows windows-msvc-headless Debug/Release、Ninja Multi-Config、MSVC、最多 4 jobs；只验证平台通用与编译/ABI，不声称 native/GPU/Linux/system IME/AT。
