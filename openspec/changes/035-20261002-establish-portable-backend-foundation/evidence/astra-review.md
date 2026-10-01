# Astra xhigh 审查结论

日期：2026-10-02。模型：`gpt-6-astra`，reasoning effort：`xhigh`。只读审查：研究文档 + 当前实现；035 proposal/design/specs/tasks；共同事务与 Recording 实现。审查 agent 没有实施代码。

## 研究审查

- 路线成立，但 P0 必须具备实际无 SDL 配置/编译，不能仅移动目录或增加接口。
- 共同资源与 ordered draw 移出 SDL；HEADLESS 跳过 SDL/libdecor/shadercross/DXC/系统字体。FT/HB/utf8proc 是真正共同依赖，保留字体字节加载。
- Quad/Glyph 已在 Core 打包 NDC，RoundedEffect 为混合 NDC/pixel；冻结 ABI v1 后再做未来 logical scene 迁移。
- Gallery 已通过私有 dirty invalidation 补偿 commit 失败；应集中共同事务，不能声称现有 Gallery 必然丢失更新。
- deferred 不应无条件 request 形成忙循环；区分待呈现 revision 和立即 wake，恢复需要明确事件。
- GPU 创建/claim 从宿主移到 renderer binding；宿主失败与 GPU 失败独立。
- Recording 必须复制和验证 bytes/texture、owner/epoch、范围、真实 draw 顺序，不能只记录调用次数。
- Core 内部 include、传递链接、真实无 SDL 构建需要共同守卫。
- async Clipboard、DOM/JNI/UIKit、无障碍桥接、Web shader/第二真实 GPU renderer 均后续处理，不在本次扩张。

## 规划复审

035 可以进入 apply，补充以下合同后实施：

1. epoch 替换先使附件失效、旧 device 存活时 retire/release，再销毁 binding；已失去设备的 handle 只丢弃，不通过新 device release。
2. Core 既有 FrameRequestState/DirtyQueues 请求自动唤醒 callback pump；deadline 提前/撤销重新排程。验证真实属性/动画路径。
3. 失败事务后附件不可呈现，成功重试才恢复；无需为 P0 增加完整 GPU 双缓冲。
4. callback 使用独立生命周期 token，pump 析构后的旧 callback 不读已释放对象。
5. 明确非空 renderer 列表的编译期组合，不实现运行时选择器。

实现证据在阶段记录中单独保存；审查结论不作为构建、运行、GPU 或新平台支持证据。

## 代码复审与修复

- 基类 attach 未虚派发会遗漏 SDL scene 缓存：改为 virtual/override；原生验收通过基类调用。
- 三类资源初建/扩容 upload 抛异常泄漏临时 handle：增加异常释放，Recording 初建/扩容 live-resource 断言通过。
- rollback 恢复 dirty queue 分配失败会丢重试：先置持久 retry 标记，恢复信息失败仍使附件不可呈现；下一次提交前完整重建 dirty。
- epoch 重建中 Effect 创建失败可能留下空资源：同步入口检测不完整资源并重新构造。
- 示例保存 Glyph/Effect 引用会跨 retire 悬空：使用 SceneResources 查询当前资源。
- Recording tombstone 保留已退休资源大块数据：释放 bytes 容量，保留 handle 元数据拒绝旧引用。

修复后的共同测试 20/20、相关原生合同 13/13；见 scene-transaction.md。

## 调度复审

复审发现活动 submit 中销毁 pump 的 use-after-free，以及真实 Button/Input 动画 dirty 请求压过未来 deadline。修复独立 token 存活检查，将本帧动画 dirty invalidation 与显式下一 epoch request 分开，RAII 保存实际消费 revision。测试覆盖 completion 显式请求和晚到更新、执行中销毁/异常与真实组件运动。

Astra 对最终修复只读复核，未发现仍需修复的实质遗漏；其结论不冒充重复运行测试。主 agent 实际共同 21/21、相关 native 19/19，详见 frame-pump.md。
