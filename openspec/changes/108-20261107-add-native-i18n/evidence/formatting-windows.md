# 精确数字与组件格式语言

2026-11-07，Windows/MSVC，`windows-msvc-headless`，Ninja Multi-Config，默认栈，最多两个 compiler。

共享 number helper 提供纯函数与捕获作用域的 `formatNumber`，整数和 Decimal 不经 double。Statistic、InputNumber 和 ColorPicker 消费独立 formatLocale，保留显式 formatter/parser/separator、负零和业务字符串。数字编辑冻结草稿、composition 和活动选区的格式上下文；清洁显示重格式化保留编辑器与 history。

## 实际自动检查

- Debug/Release 正式构建各 18 个相关 target；`rynui.i18n_components_formatting` 含 9 组真实组件场景，另覆盖 InputNumber、Statistic、ColorPicker、Timer、公开 i18n、共享 editor/history/reconcile 与 Form 数字绑定。
- Debug 28 个不同入口均通过：26 项首轮通过；保留 Statistic 原有非有限数值错误文案、修正 ColorPicker 测试的弹层查询范围后，两个失败入口及相关数值合同复验通过。Release 最终单轮 28/28，0 失败。测试源码的公开 API 调用已在实际 MSVC 编译中校正。
- 补充真实失焦回归：动态 changeOnBlur getter 销毁组件后，旧处理不访问状态或继续调用 blur 回调。
- clang-format 22.1.3 检查 1135 个 tracked 源文件，0 failures；新增三个 C++ 文件另经 dry-run。doctor healthy、strict 108/108、diff check 通过。

正式 presets：`windows-msvc-headless-debug` / `windows-msvc-headless-release`；在 MSVC Developer Environment 中构建相关 targets 并按下列 CTest 范围执行：

```text
^rynui\.(i18n_(components_formatting|catalog|context|public_api)|input_number_(model|component|public_api|theme)|statistic_(model|theme|retained|formatting|clips|lifecycle|gates|boundaries|rollback|public_api|docs_example)|popup_statistic_(locale|timer|statistic_motion|callback)|color_picker_component|portable\.text_(editor|history|reconcile)|form_bound_control)$
```

实际注册的 Timer 动画入口为 `rynui.popup_statistic_statistic_motion`，选择时使用该完整名称；原始精确选择式、日志和构建输出仅在 ignored `out/resume-20261107/`。这些是 HEADLESS 通用逻辑结果，不代表 Windows native GPU、系统 IME 或 Linux 验收；108 的语言切换平台任务仍独立追踪。
