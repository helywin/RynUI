# Modal API 与受控状态模型增量

2026-10-23，实际 Windows/MSVC x64，`windows-msvc-headless` Debug/Release。新增独立可包含的 `ryn/modal.hpp`、typed Props/Mask/取消原因/Content/Title/Footer slots 和受控模型；没有实现可挂载对话框。

模型只允许已显示且仍请求open时发出请求；父级拒绝关闭不改变显示。确认加载不阻止取消；默认页脚/close/keyboard/mask取消入口独立门控。显示转换与构造分离，初始隐藏没有关闭通知，owner隐藏保留受控open，重复settle没有重复转换，实际关闭使用optional(false)而非丢失false事件。非法width/footer/取消原因拒绝，配置更新在验证前不覆盖旧值。

```text
Debug API/model CTest: 2/2 passed (0.03s)
Release API/model CTest: 2/2 passed (0.03s)
clang-format 22.1.3: 491 owned source files, 0 failures
openspec doctor: healthy; strict: 61/61; git diff --check: passed
```

这是task2.1的API/model子增量，Token仍未完成，2.1保持未勾选；输入边界、实际Modal、Gallery和原生平台没有据此记为通过。
