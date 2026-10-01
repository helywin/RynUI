# Design

## Context

基线为 `1640127`。Astra xhigh 已只读审查研究文档与实现：Core target 已无 SDL 链接，但根配置、共同 GPU resources 和原生宿主仍耦合。Quad/Glyph 已打包为 NDC，RoundedEffect 同时携带 NDC 和 pixel 数据，不能声称现有 scene 全为 logical。

## Goals / Non-Goals

**Goals:** 在现有 Windows SDL GPU 行为上建立真实可执行的 P0 边界。HEADLESS 使用同一 Core 和共同资源同步代码，通过 Recording 验证上传数据、顺序、失败恢复与代际。后续组件默认遵守这些边界。

**Non-Goals:** 新 OS、真实第二 GPU renderer、shader 转译、自动 GPU device-loss 检测、异步剪贴板/字体提供器、DOM/JNI/UIKit、可访问性桥接。本次不重新设计 packed geometry ABI，也不重构全部示例的业务布局。

## Decisions

1. **显式后端选择。** `RYNUI_PLATFORM_BACKEND=SDL|HEADLESS`，`RYNUI_RENDER_BACKENDS=SDL_GPU|RECORDING`（可选列表）；默认 SDL + SDL_GPU，HEADLESS 禁止 SDL_GPU。建立 `windows-msvc-headless` preset，关闭原生 examples/default_fonts；只解析 FT/HB/utf8proc 和验证字体。共同 tests 可以在 HEADLESS 下编译，SDL 专用 tests 仅在 SDL 配置启用。未知/不支持组合配置时失败。保留真实文本依赖比添加伪字体实现更能证明 Core 可复用。

2. **共同 renderer 层。** 将无 SDL 的 GlyphGpuResources、RoundedEffectGpuResources、SceneDrawApi 移至 `renderer/common` 独立 target；新增 SceneBackend 合同与共同 SceneResources。低层 handle 保持内部 opaque 类型，SceneResources 绑定唯一 backend 身份及设备 epoch，附件由共同资源生成并检查归属。明确上传 bytes 在方法成功返回时由 backend 复制/持有；commit 表示命令接收成功，不代表 GPU 执行完成。纹理行布局由 backend 提供对齐限制，Core scene 不携带 SDL transfer 限制。

3. **共同上传事务。** begin → Quad/Glyph/Effect synchronize → commit；任何阶段失败/异常 cancel，并重新标脏参与同步的 CPU stores/atlas 和 effect cache。已有 Gallery 私有回滚是有效补偿，本次将其集中，避免新调用者遗漏。资源扩容后 commit 失败允许保留新 handle，重试全量重建内容。epoch 改变释放/重建资源并从 CPU 数据重传，不 remount 组件。Gallery 接入共同资源与事务，保留必要统计字段；其他 scene 示例至少复用共同事务。

4. **真实 Recording。** 实现相同上传/场景接口，复制并校验 buffer/texture bytes、范围、owner/epoch，记录有序 draw 与对应资源；提供确定性 begin/upload/commit 失败注入及显式 epoch reset/surface unavailable。失效 handle 保留可辨识 tombstone，避免裸指针重用误判；Recording 只证明数据和控制合同。用真实组件/文字/圆角效果 fixture 走相同资源路径，不以成功返回假实现代替。

5. **宿主与 GPU 生命周期。** PlatformState 只拥有 SDL init/window/input/clipboard；新 SdlGpuBinding 在 renderer 层拥有 create/claim/release/destroy，失败清理仅影响绑定。Scene/Quad renderer 使用 binding，Frame renderer 注入 binding。测试分别验证宿主与 GPU 创建/claim 失败、清理顺序、宿主继续可用。窗口必须比 binding 活得更久，renderer/resource 析构顺序明确。SDL 设备损失仅报告失败并允许显式重建，不能声称自动恢复。

6. **非阻塞 tick。** 提取 poll/deadline/submit 的 tick，与原生 step 的等待分离。callback 驱动封装合并 wake/deadline，并通过 generation 拒绝销毁/重启后的旧回调，禁止重入。deferred 留存待呈现 revision，但不自动产生立即 wake；恢复显式 request/resume，避免无 surface 自旋。接受上传后重复呈现不重复上传，动画使用同一单调 now。现有原生 loop 继续按 deadline 等待。

7. **实际守卫。** 扫描 Core 内部 include，拒绝 backend 与 OS/SDK headers；检查共同 target 的传递依赖；HEADLESS 的实际配置、编译、link closure 和 compile commands 证明没有 SDL/shader/default-font source。负向 fixture 验证非法选择和边界违规确实失败。

8. **packed scene ABI v1。** 正式文档固定当前 float packing、屏幕 logical 原点左上、NDC x 右/y 上、clip/translation、UV 左上和颜色/混合语义；RoundedEffect pixel packing 在共同 renderer conversion 完成。未来 backend 在消费边界转换 API 特有坐标/格式；完全 logical scene 是后续可独立迁移项。保持现有 static_assert 与 shader ABI tests。

## Risks / Trade-offs

补充 Astra 对规划的审查合同：epoch 替换前先使附件失效，在旧 device 活着时 retire 旧资源，再销毁 binding；若设备已消失，只丢弃旧代际记录，不得经新设备 release。失败事务后共同资源不可呈现，直到成功 commit。callback 使用独立生命周期 token，析构后旧 callback 不得访问 pump；既有 Core frame requests 自动唤醒，deadline 提前/撤销会更新排程，并用真实属性/动画 fixture 验证。编译期选择矩阵详见 `docs/renderer-contract.md`，不实现运行时多后端选择器。

- 内部 API 迁移影响 fake 与源代码合同 → 更新真实合同并跑既有 suite，不扩大无关计数断言。
- rollback 标脏可产生失败后的全量上传 → 仅失败/epoch reset 路径承担该成本，idle 与局部更新仍验收。
- Recording 无 GPU 视觉覆盖 → Windows 实际窗口、DPI、字体/输入、截图与 Release 验收独立。
- HEADLESS 仍需要文本依赖和验证字体 → 明确这是 backend isolation，不宣称无第三方依赖。
- 无 Linux 机器 → Linux 原生任务保持 pending，平台通用逻辑只在 Windows 验证一次。

## Migration Plan

规划提交后按 tasks 实施独立阶段：构建/共同层、宿主 binding、事务/Recording、调度、集成与验收；每阶段独立验证并提交。默认 preset 保持现有桌面入口。回退使用阶段提交，CPU scene/公开组件 API 无持久化格式迁移。研究文档区分当前已验收 P0 与未来 P1+，不把未实现平台表转成支持承诺。
