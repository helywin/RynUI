# 多行编辑与光标基础

2026-10-03，Windows/MSVC，windows-msvc-headless Debug/Release。TextEditMode::MultiLine 统一初值、编辑、formatter 和 authoritative 值的 CRLF/CR → LF；默认 SingleLine 继续删除换行。覆盖 Unicode/grapheme/scalar/byte limit、formatter 重入/无效 UTF-8、历史回放和 controlled echo。

TextCaretMap 的实际行测量重载覆盖软折行/硬换行、空行/末尾 LF、ligature/CJK/combining/emoji、二维命中、upstream/downstream、行边缘及保持期望 x。无效行几何和注入 bad_alloc 保留最后完整发布的 map；20,000 轮多行查询无分配。TextSceneService 回归验证窄/宽 reflow 不重复 shape，清空后剩一个合法空行。

affected CTest Debug 8/8（94.12 s），Release 8/8（6.16 s）；新增 scene reflow 断言后该 Debug 测试再跑 1/1。input_scene_allocation 保持既有选择/组合选择零分配合同，92.62 s 是 Debug 测试运行时间，不是性能提升声明。text_caret_map 原先只在 native suite，现加入 portable CTest。

clang-format 22.1.3 检查 434 个自有文件 0 failure，OpenSpec doctor healthy、strict validate 53/53、git diff --check 通过。本阶段尚未新增 TextArea 公开组件或原生窗口证据；后续任务接入。
