# Form 消息来源与语言重投影

2026-11-07，Windows/MSVC，`windows-msvc-headless`，Ninja Multi-Config，默认栈，最多两个 compiler。

`.required()` 与缺省 rule message 保存 builtin 来源，显式 String 保持 literal。当前反馈和查询按最新语言投影；已交付 FormResult 保持快照。公开 optional message 迁移见 `docs/form.md`。

## 自动验证

- 正式 Debug/Release 各构建 14 个相关 target。新入口 `rynui.i18n_components_form` 含 8 组真实 Host/Provider 场景，两个独立公开头 TU 与同一 target 编译，不重复计数。
- Debug 15 个不同 CTest 均通过：13 项首轮通过；修正新测试的异步取消基线、退役通知断言和文档 UTF-8 编解码后，其余 2 项复验通过。Release 最终单轮 15/15，0 失败。
- 覆盖 builtin/literal、历史结果、嵌套与双窗口、有效异步请求、重入销毁、受控拒收、晚到完成和 dirty/validate-only；保留 Form 旧模型、交互、输入域、Modal、Tree 组合与 Theme 合同。语言变化不重跑业务校验，不增加 deadline。
- clang-format 22.1.3 检查 1132 个 tracked 源文件，0 failures；新增 C++ 另经 dry-run。doctor healthy、strict 108/108、diff check 通过。

在 MSVC Developer Environment 中使用正式 build/test presets；CTest 选择式：

```text
^rynui\.(i18n_components_form|form_(public_api|model|async|component|theme|advanced|interaction|lifecycle|integration|modal_scope|input_domain|bound_control|documentation)|tree_form_integration)$
```

原始构建、CTest 和静态检查输出仅在 ignored `out/resume-20261107/`。本阶段为平台通用合同在 Windows 上的实际验证，不代表 native GPU、系统 IME 或 Linux 验收；对应平台任务保持独立。
