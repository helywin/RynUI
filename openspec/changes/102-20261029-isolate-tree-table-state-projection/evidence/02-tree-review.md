# Tree 阶段独立复核

2026-10-30，固定 main `f595211e753f8d117dd72f1390183515a9577b31`，直接父 `033bc96bae1dbe5ee028a04f604fba074ed1cfea`。全新 Astra / ultra / priority 审查结论 CLEAN，仅覆盖 task 1.1/1.2；不代表 Table 或 102 整体完成。

完整父差异及相关源码复核确认：hover/focus 只排入相关行，固定 effect 槽保留，结构、Theme、disabled、checked、geometry、clip、opacity 的失效路径未丢失。队列置位和成功发布后清理的异常顺序成立，历史断言未修改。

独立直接运行 ROOT 现有程序，Debug/Release 各 14 次，共 **28/28 exit 0**：state_projection 的 tree_local/tree_invalidation/tree_retry；Tree component 的 controlled/rich/reactivity/lifecycle；Tree review 的 focus_clips/zero_area；Tree R2 的 nested_wheel/throwing_blur_rollback；TreeSelect component 的 keyboard/retained；glyph_replace 无参。12 个 EXE 的 SHA-256、大小、mtime 前后保持，8 个源码哈希、103 个冻结记录及 18 轮原始 Tree 数据与索引一致。

这是 existing-binary 复跑，审查没有构建、CTest、benchmark、GUI 或远端操作。实际故障注入 Debug 5 次、Release 8 次，过滤条件为至少 64B 请求，不扩展为所有 OOM 可恢复；work 计数也不包含 O(N) intact 等访问。

父线程随后通过临时禁用本地失效代理的单次 Git 命令完成 fetch/push，远端 `refs/heads/main` 精确读回 `f595211e753f8d117dd72f1390183515a9577b31`；未更改全局代理或 TLS 校验设置。原生/GPU/Linux/IME/AT 和整页 240 Hz 体验仍待验。
