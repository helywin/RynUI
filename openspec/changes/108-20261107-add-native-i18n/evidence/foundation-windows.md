# i18n 基础 Windows 验证

Windows/MSVC，正式 `windows-msvc-headless` Debug/Release、Ninja Multi-Config、最多 2 jobs；默认栈及编译选项保持。原始输出仅在 ignored `out/108-i18n-foundation/`。

交付 immutable 语言/消息目录、完整 zh-CN/en-US 数据、typed 参数/整数复数/fallback/诊断、透明 ConfigProvider、窗口隔离与捕获上下文、公共 I18n binding/事件快照、公开应用定义。现有 Locale 类型集中到纯数据头并保留旧声明名称/aggregate 前缀；不新增第三方依赖，不改 Theme golden。

实际 `cmake --build --preset windows-msvc-headless-debug` / `windows-msvc-headless-release` 构建 `rynui_i18n_catalog_tests`、`context_tests`、`public_api_tests` 与 Prop/reactive_source_lifecycle/Theme scope/runtime/algorithm/allocation portable targets。CTest 同范围：Debug **9/9**、Release **9/9**；Debug 架构边界另 **1/1**。

已实际覆盖完整包/回退/schema/UTF-8/复数、语义等值、普通/透明/before-child/append、两窗口、24 层配置、10k 等值、重入/退役/非法恢复、真实 Text/Input 保留身份与编辑状态、公开头应用。修正了测试中 Color 名称歧义及公开例子消息常量与 Model 成员重名，不改变 API 语义。

clang-format 22.1.3 检查 1076 个已跟踪自有源文件及新增源码，0 失败；doctor healthy、strict 108/108、diff 检查通过。

仅完成 108 阶段 1。旧控件默认文案/配置消费、数字日期 helper、Form 消息来源、App 服务、完整 Gallery 双语和原生 GPU/OS 集成仍未完成。CPU fixture 不能替代真实窗口、系统 IME、Linux 或 GPU 证据；107 ConfigProvider 整项保持开放。
