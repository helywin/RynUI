# 状态内容与焦点 API 证据

2026-10-02，Windows/MSVC，Ninja Multi-Config，windows-msvc-headless Debug/Release。

- 新增 Switch public header 单独编译合同，覆盖 typed direction、Signal、不同 typed slots 和未绑定 ref。
- 新增 Switch features 合同：两种文字内容各执行一次、受控值切换不测量/shape、不改变自然宽度；inactive 文字更新测量，RTL/窄约束/零内容预算恢复、祖先 translation 和 glyph clip；保留 Checkbox/Radio 兄弟。
- 覆盖 ref/autoFocus、跨线程拒绝、disabled/loading、callback 顺序与受控未回写、onChange 内销毁与 onClick 副本、重绑定、重复绑定及嵌套交互 mount 回滚。
- 连同既有 selection_component、radio_component、text_component，Debug 5/5（1.83 秒）、Release 5/5（1.31 秒）通过；命令与日志为 out/046-switch-stage1-final.log。
- clang-format 22.1.3 全仓自有 402 文件检查，0 failures。

此阶段不包含 2.1 的主题新 token、手柄阴影/伸展和 wave，也不代表真实窗口/GPU 验收。
