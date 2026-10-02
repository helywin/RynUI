# 共用 API、焦点与系统提示验收

实际平台：Windows，Visual Studio 2026 MSVC，Ninja Multi-Config。使用 windows-msvc-headless-debug/release。

- Debug：9/9 CTest，4.58 秒。
- Release：9/9 CTest，3.17 秒。
- 测试项：input_public_api、input_component、text_input_session、password_component、search_component、focus_order、focus_state、focus_lifecycle、typography_interaction。
- input_component 新合同覆盖未绑定/复制共享/卸载再绑定/跨线程引用，UTF-8 grapheme 边界、Keep/Start/End/All，首次布局 autofocus、初始 disabled 和活跃分支，焦点回调延后转移及自卸载，disabled/branch blur 自卸载，嵌套槽重复绑定回滚，Password/Search 系统提示透传和密码强制提示，IME 延后会话刷新及旧 stamp 拒绝，选择取消平台 preedit。
- 共用 fluent API 保持返回具体 Props 类型；没有新的 renderer/platform 依赖。

复现：通过 VS Developer Environment 构建 rynui_portable_<上述名称>，对应 CTest preset 使用 `-R "rynui.portable.(input_public_api|input_component|text_input_session|password_component|search_component|focus_order|focus_state|focus_lifecycle|typography_interaction)$" --output-on-failure`。

这验证平台通用端口合同；不是实际系统输入法 UI、GPU 或 Linux 验收。四变体、统计和动作配置仍按后续任务实施。
