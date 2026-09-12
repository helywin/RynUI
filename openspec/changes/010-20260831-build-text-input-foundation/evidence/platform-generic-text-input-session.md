# 文本输入会话桥接：平台通用验证

日期：2026-09-12。平台：Windows 11 / MSVC 19.51.36256 x64；正式 `windows-msvc`、`windows-msvc-debug` preset，Ninja Multi-Config。

## 实施范围

- 每个 window port 与 editor store 均限制单一 session host；owner generation 与递增 session epoch 防止跨焦点周期、销毁复用后的迟到事件写入。
- FocusManager 的 Tab、pointer focus、window focus 与 Component 条件卸载通过实际 headless lifecycle fixture 验证；disabled/read-only 变化立即清除 composition 并停止 native session。失败的 stop 必须重试成功后才可启动下一 owner。
- SDL adapter 复制 UTF-8 committed/preedit/candidate snapshots，保留 character/scalar range、未指定 range、candidate selection/orientation 与键盘 repeat；过滤其他 window 和会话启动前的 native timestamp。
- 无效 payload、range、容量或分配失败不部分追加队列；错误通过 exception 及 rejected-text diagnostics 暴露。
- SDL 3.4.14 adapter 接入 text type/capitalization/autocorrect/multiline=false；logical bounds/caret 经 translation、clip、scale、outward rounding 和 window clamp 转换，same-value elision，window focus 恢复后重新提交 area。测试覆盖 1.0/1.25/1.5/2.0 scale、scroll 与失败重试。
- composition/candidates 与 committed revision 分离；中文、韩文、emoji、candidate-first/empty-after-commit 顺序、取消、blur、destroy、外部 conflict 与相同值 echo 已覆盖。commit 仅执行一个 committed mutation。history 的接入仍属于阶段 4。

## 验证

- 正式 Debug 全量增量构建通过。
- 全量 CTest：184/184 通过（138.28 秒）；随后对最终补充的 window-port 独占检查和 rejected-text diagnostics 重跑受影响测试，18/18 通过（1.32 秒）。
- `text_input_atomicity` 对 preedit、candidate copy、SDL-shaped candidate queue preparation 的每个分配点逐一注入失败，直到完整成功，验证旧 value/selection/candidates/queue 不变。
- 注入测试暴露 MSVC Debug 标准库在标为 noexcept 的 vector/string 默认或移动构造中分配 iterator proxy；事件 snapshot 与 `ryn::String` 改为可抛错的准备后 swap，UTF-8 copy 使用保证消除临时复制的 prvalue 返回，不再 terminate。
- 既有 editor benchmark：10,000 cycles / 90,000 operations / 0 allocations；owner identity/capacity 不增长；103 个 mutation preparation 失败点仍保持原子性。
- 全量回归补齐 shadercross SYSTEM fixture 的 utf8proc 2.11.3 假包，未改变生产依赖解析模式。
- OpenSpec doctor、strict validate 与 `git diff --check` 作为阶段提交门禁执行。

## 验收边界

以上是平台通用及 SDL-shaped/fake platform contract，并非真实系统 IME、候选窗视觉或 GPU/DPI 人工验收。Windows 与原生 Linux Wayland 清单保持独立且未勾选。3.7 的系统 placeholder/default/max length 属性继续按用户批准延期，不阻塞后续实现。
