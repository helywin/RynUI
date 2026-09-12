# 剪贴板桥接与编辑命令验证

2026-09-12，Windows 11 / MSVC 19.51.36256 x64，正式 `windows-msvc` / `windows-msvc-debug`，Ninja Multi-Config。

- `TextClipboard` 通过 owned UTF-8 snapshot 交换文本，区分空字符串、无文本、平台失败、无效 UTF-8、容量和分配失败、错误线程。单次 text payload 上限 1 MiB；写入包含 NUL 的值显式拒绝，不经 C API 静默截断。
- SDL3 main-thread adapter 使用 `SDL_GetClipboardText` / `SDL_free`，所有验证与分配失败路径均释放平台缓冲区。`SDL_EVENT_CLIPBOARD_UPDATE` 仅保存 owner 与格式数量的值快照，不保留 MIME 指针，不把 metadata 当作未来命令时的内容保证。
- copy/cut/paste 使用命令开始时的 owner generation、revision 和选区；平台回调后重新查找 owner。选区变化不改用另一段文字，value 冲突或 destroy/reuse 则拒绝覆盖。
- read-only 可复制，拒绝剪切/粘贴；空粘贴不删除选区；正常粘贴移除 CR/LF，以一次 mutation 接入 scalar maxLength 和 grapheme-safe truncation。
- 正式 Debug 构建通过。桥接及事件/demo/platform 回归 13/13 通过；命令接入后 editor/session/clipboard/dependency 回归 10/10 通过。
- `text_clipboard` 逐点注入 snapshot 分配失败，校验平台缓冲区无泄漏；写入准备失败不调用平台写接口。`text_clipboard_commands` 覆盖平台失败原子性、读取期间内容/选区变化、外部 value 冲突、销毁复用及错误线程。
- OpenSpec doctor healthy、strict validate 10/10、`git diff --check` 通过。

全部剪贴板验证使用注入的 fake platform，未读写用户真实剪贴板。系统 Windows/Wayland clipboard 人工与真实窗口验收仍留在各自平台清单。history 合同仍由 4.3–4.5 单独接入和验证。
