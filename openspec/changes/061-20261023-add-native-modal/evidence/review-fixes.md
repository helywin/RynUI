# Astra 审查修复

2026-10-23，Astra xhigh / priority 审查 `4e258e2`，识别两项问题并补充回归覆盖：

- 背景受控 Tooltip 曾拒绝关闭后，相同关闭请求会跳过边界隐藏，继续消费 Escape。强制可见性同步与回调去重分开；保留边界抑制，解除边界不复活旧浮层，显式新打开或新交互可以恢复。
- Slider dots 使用可变 content range，未经过 Modal 正文裁剪。content range 现在以保留的圆角 fill effects 应用祖先 clip；动态数量变化回收旧 effect，原 quad 数据和 CPU/GPU ABI 保持不变。

Gallery 集成另发现隐藏 Select 的窗口层定位每帧执行几何失效。隐藏状态同步改为仅在可见状态变化时标记几何；关闭 Modal 中的 Select 不再持续请求帧。

验证环境：实际 Windows / MSVC x64，`windows-msvc-headless`，Debug 与 Release 各 4/4：`modal_component`、`modal_boundary`、`select_component`、`tooltip_component`。新增拒绝关闭后两次边界切换、显式重开、dots 滚动裁剪/销毁、隐藏 Select idle 回归。最终 Debug 2.70s，Release 1.30s；增量回归日志位于本机 `out/modal-review-*-tests-final.log`。格式检查496个文件、doctor healthy、strict 61/61、diff check通过。

这组修复没有增加 Linux、GPU 原生窗口或人工系统 IME 验收结论。
