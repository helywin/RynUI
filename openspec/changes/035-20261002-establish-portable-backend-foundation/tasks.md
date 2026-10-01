# Tasks

平台通用验收在 Windows/MSVC 完成一次，使用 `windows-msvc-headless` 与必要的默认 preset。原生平台证据独立提交。每阶段运行相关 build/CTest、`openspec doctor --json`、`openspec validate --all --strict --no-interactive` 和 `git diff --check` 后提交英文 commit；证据保存在本 change 的 `evidence/`。

## 1. 构建与共同 renderer 边界（平台通用）

- [x] 1.1 提取共同 GPU resources/draw 合同至独立 target，更新调用者和 resource tests；默认 MSVC 构建及相关 CTest 通过。
- [x] 1.2 增加显式 backend 选择、HEADLESS preset、无 SDL/shader/default-font 解析路径；HEADLESS 实际配置/编译并检查构建图与 compile commands，非法组合负向测试通过。
- [x] 1.3 增加内部 Core include 与传递依赖守卫，固定 packed scene ABI v1 文档；负向泄漏 fixture 和现有 ABI tests 通过，并提交本阶段。

## 2. SDL 宿主与 GPU 所有权（平台通用）

- [x] 2.1 将 GPU create/claim/release/destroy 移入 renderer binding，迁移 renderer 与示例，宿主只拥有窗口和服务；生命周期 fake tests 覆盖设备/claim 失败后宿主可继续使用。
- [x] 2.2 明确 binding/host/resource 析构顺序和显式重建边界，更新架构；默认 MSVC 构建、platform/frame tests 通过，并提交本阶段。

## 3. 场景事务与 Recording（平台通用）

- [ ] 3.1 实现共同 SceneBackend/SceneResources 上传事务、失败回滚及 owner/epoch 附件检查；begin、部分上传、commit、异常与扩容失败测试通过。
- [ ] 3.2 实现复制真实 bytes/texture、验证范围与 draw 顺序的 Recording renderer；真实组件/字体/效果 fixture 验证内容、idle/局部上传、跨 owner/旧 epoch 拒绝与 CPU 重建。
- [ ] 3.3 将纹理上传对齐限制放在 backend 合同，Gallery 和其他 scene 示例接入共同事务；SDL 默认构建和相应资源/源合同测试通过，更新文档并提交。

## 4. 非阻塞帧调度（平台通用）

- [ ] 4.1 提供原生 step 共用的非阻塞 tick 与 callback wake/deadline/generation 封装；测试验证无 wait、wake 合并、重入/旧 callback 拒绝与单调时间。
- [ ] 4.2 明确 deferred 的待呈现 revision 与恢复，不无条件立即重试；Recording surface 挂起/最新内容恢复/无重复上传和既有 scheduler tests 通过，更新文档并提交。

## 5. 集成验收（平台通用）

- [ ] 5.1 HEADLESS Debug/Release 构建与共同 CTest、默认 preset 的完整 CTest 通过；只新增影响范围相关检查，记录实际 suite 结果与失败边界。
- [ ] 5.2 更新正式架构、AGENTS、README 与研究文档的已实现/未来范围，保存 Astra 审查结论及证据索引；doctor、全量 strict、diff check 通过并提交。

## 6. Windows 原生验收

- [ ] 6.1 在 Windows/MSVC 默认 Debug 和 Release 上实际运行 SDL Gallery，验证窗口/GPU/字体/输入服务、resize 与 DPI 场景；保存日志和截图并独立提交 Windows 证据。

## 7. Linux 原生验收

- [ ] 7.1 在真实 Linux/GCC 或 Clang 上构建并运行 SDL Gallery，验证窗口/GPU/字体/输入服务与 resize/DPI，保存证据并独立提交；无实际 Linux 环境时保持 pending。
