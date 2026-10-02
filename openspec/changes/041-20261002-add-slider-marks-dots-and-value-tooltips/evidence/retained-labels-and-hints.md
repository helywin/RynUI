# 标签、点与提示的平台通用验证

2026-10-02，Windows/MSVC，`windows-msvc-headless-debug` 与 `windows-msvc-headless-release`。此处验证平台通用逻辑；真实窗口/GPU/系统字体证据独立记录。

- Debug 完整 CTest：38/38，85.12 秒；Release 完整 CTest：38/38，12.24 秒。Core/renderer 边界守卫均通过，Input allocation 基准无回归。
- Slider focused 合同覆盖真实 CJK/英文 Text 标签和点击、marks 动态增删/清空、保留 thumb/Tooltip identity 与焦点、dots/included、Auto/Always/Hidden、Escape、新焦点重开、disabled、controlled 提示只使用已回写值、formatter 缓存及重入销毁。
- 新内部 append_slot 挂载失败时回滚组件、Text scene、交互和 host 列表；不重新运行根 Content。失败回滚与 sibling 保留有运行测试。
- 动态缩减 labels 时先缩减索引表，再销毁旧子树，防止 cancel/leave 回调使用缩短后的 marks 越界。MSVC Debug 断言输出到 stderr，避免 CI 阻塞在运行库提示框。
- marks/点颜色更新只失效 material；mark 字号/行高/间距更新重新测量。字体族/字重沿用 Typography Theme；Tooltip 的字体组失效传播也有回归测试。新增 dot/mark token 进入 JSON/hash/继承/算法，五份 Theme golden 已重新生成。
- Gallery 在现有五个 Slider 样例中展示标签、离散范围、dots/included 与值提示；保留 partial，并将剩余范围具体限定为整段轨道拖动和多端点编辑。公开文档同步。
- `python scripts/format-code.py --check`：clang-format 22.1.3，395 个自有源文件、0 failures；doctor healthy；strict 41/41；`git diff --check` 通过。

Windows 原生任务和 Linux 原生任务分别记录；此证据不声称 Linux native 或用户人工输入验收通过。
