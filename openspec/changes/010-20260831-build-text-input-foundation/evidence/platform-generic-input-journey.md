# 平台通用：完整 Input journey

2026-09-12；任务 8.1；Windows/MSVC，正式 `windows-msvc-debug` preset。

`tests/input_journey_tests.cpp` 通过实际 Input host、Focus keyboard routing、TextSceneService 和 CountingGpu 串联受控与非受控流程，分别输出 19 / 18 个阶段的 value byte count、selection、composition、history、input-area、shape、scene 和 upload diagnostics。

覆盖 Latin、中文、family ZWJ emoji、preedit/candidates/commit/cancel、select/copy/cut/paste、undo/redo、换行归一化、Unicode scalar maxLength、prefix/suffix、Theme/status、read-only/disabled、submit、caret blink、受控 echo 与 external reconcile。逐阶段断言 authoritative value、selection 范围、owner/component/text identity、slot 单次执行、dirty drain 和固定 3 Quad / 19 effect topology；selection-only 更新不重新 shape。

销毁后验证旧 session event 被拒绝、start/stop 配对、editor 释放、frame/deadline 回到 idle。独立 single-mount fixture 强制复用 editor index 并更换 generation，聚焦新 owner 后仍拒绝旧 commit/composition，避免错误地重复调用一次性 mount 接口。

新增 internal `InputComponentHost::synchronize_input_area`，在 layout/scroll 后将 viewport、ancestor clip 与实际 caret 位置交给既有 session mapper；window coordinate scale 与 glyph/display scale 独立。四档 1.0/1.25/1.5/2.0 验证长文本 End scroll、Home、clip、向外取整、same-value elision、平台失败重试和窗口失焦。Home 的相对 cursor 可因向外取整为 1 像素，并非总为 0。

定向 session、Input component/pointer/keyboard/caret/journey 共 6/6 通过（5.06 秒）；其中完整 journey 0.35 秒。此为平台通用 headless/CPU/fake GPU 证据；真实窗口事件接入、系统 IME 候选窗和人工视觉验收仍由后续任务独立完成。
