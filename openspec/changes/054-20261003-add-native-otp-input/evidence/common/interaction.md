# OTP 导航、IME 与生命周期

Windows/MSVC/Ninja Multi-Config，`windows-msvc-headless` Debug/Release。

最终 OTP model/component/public API/interaction、reactive source lifecycle/Prop、focus order/state/lifecycle、interaction/pointer、text session/clipboard/editor/reconcile、Input/count/Password/Search/TextArea/Typography：两配置均 22/22，Debug 8.57 秒，Release 5.30 秒，日志 `out/054-interaction-tests.log`。

OTP interaction 覆盖：mount-only autofocus、ref 线程/复制/退休、点击及实际 indexed focus/blur、第一空格重定向、左右/RTL/空 Backspace、正常 Tab、primary undo 拦截；preedit 不分格/前进且拥有导航，mask hint 延后刷新、native area 与 stale stamp；active tail shrink 保留前缀并取消旧 owner，显式 blur 焦点优先；readonly copy/拒绝 cut/paste、masked copy/cut、disabled；初始 formatter 改写 Props、formatter throw/缩短容量/authoritative 冲突/卸载、partial callback 冲突/同值 echo/卸载及 clipboard callback 卸载。

动态删除暴露 ReactiveSource 借用指针生命周期问题：observer 可能仍存活而其 source 已析构。现在 source 析构通知 observer 移除该依赖；直接 regression 验证销毁后重算、observer 内销毁、queued notification 与 cleanup 零分配。Debug 原始崩溃 stack 在 out 诊断日志，临时 Windows 调试代码已移除。

clang-format 22.1.3：448 自有文件通过；doctor healthy；strict validate 54/54；diff check 通过。此为共同逻辑与端口测试，不代替真实 SDL/GPU/OS 候选窗口/Linux 验收。
