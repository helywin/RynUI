# Input 字形层同步

只改 Input 字形提交阶段：完全裁剪层和透明层调用既有 synchronize_culled；全部输入几何、选择/caret/preedit、onResize 和 editor 更新仍执行。隐藏层不必初始化 glyph range，已有 range 保留且 clip 为空；恢复时使用最新脏内容和材质。

`culled_input_layers_restore` 检查实际 glyph 可见性、不可见层零 synchronize、连续八次裁剪更新无重复 glyph sync/patch、屏幕外 authority 更新、同 editor/完整组件代际、选择恢复及 preedit/session 保留。原滚动全量重建对比继续检查可见正文的位置/UV/颜色/clip；透明 placeholder 改由新测试实际切换后验证，不再要求透明层必须分配实例。

官方 MSVC / Ninja Multi-Config / windows-msvc-headless，最多 2 jobs：

- Debug 新构建 28.0532175 s；11 项 CTest 全过（5.1653855 s），另直接执行 Date form_reset、Time reset 两项均过（2.0690333 s）。首次正则未匹配两个 reset 的下划线名字，已更正并补跑，未将 11 项说成 13 项同次 CTest。
- Release 新构建 35.4859902 s，13/13 CTest 全过（3.8118693 s）。
- 两配置覆盖 Input component/GPU/caret/display/actions、TextScene、TextArea、TextInputSession、Select、Focus lifecycle、Form 和两个 picker reset。
- 格式检查 804 文件通过。首次编译的同名局部变量冲突已更名修复，原诊断日志保留。

原始日志在相邻 artifacts；本阶段不宣称原生滚动性能或 Gallery 集成通过。
