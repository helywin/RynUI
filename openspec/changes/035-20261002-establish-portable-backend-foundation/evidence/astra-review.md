# Astra xhigh 审查结论

日期：2026-10-02。模型：`gpt-6-astra`，reasoning effort：`xhigh`。两次只读审查：研究文档 + 当前实现；035 proposal/design/specs/tasks。没有实施代码。

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
