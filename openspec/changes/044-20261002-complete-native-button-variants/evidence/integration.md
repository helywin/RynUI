# 整合阶段证据

2026-10-02，Windows x64，MSVC / Ninja Multi-Config。任务 5.1 平台通用整合完成。

- `windows-msvc-headless-debug` 完整 build/CTest：41/41，89.41 s。
- `windows-msvc-headless-release` 完整 build/CTest：41/41，14.23 s。
- 上述包含 Core/renderer include/link configure 守卫、实际 HEADLESS 构建及新 Prop 合同。
- `windows-msvc-debug` Gallery frame/catalog/document、Theme algorithm/runtime/public/allocation、Button public、Prop：9/9，20.59 s。
- `windows-msvc-release` 相同回归：9/9，5.57 s。
- clang-format 22.1.3 check：397 个已跟踪自有源文件，0 failures；新原生验收文件单独格式化。
- 支持目录 generator check/self-test、五个 Theme golden 增量合同、doctor/full strict/diff 通过。

Gallery 增加 25 个保留样例，总计 95 个 live samples；Button 的支持状态按原生功能更新，Web HTML/DOM/CSS/React API 保留边界说明。回归验证交互与主题更新保持组件、节点、交互和 scene 身份；wave 增减 effect 可以更新绘制拓扑，但 settled idle 不重复重建或提交。

Windows Debug 原先因递归 composition 按值保存新增大型 Theme 配置而触发默认栈溢出。大于 256 bytes 的静态 `Prop<T>` 改为不可变共享存储；小值继续内联，绑定路径保持原有语义。未增大进程栈。新合同覆盖存储上限、静态副本隔离、reactive 更新及 Scope 销毁；实际 Gallery Debug 运行已通过。平台通用 Prop 也纳入 HEADLESS CTest。

Theme golden 增加八个 Button 字段并更新 identity。`validate_goldens.py` 对提交 `226a6d8` 的既有 JSON 字段逐项比较，五种算法顺序的原值全部保留。依据锁定 Ant Design 6.6.5 variant 源码补正 Filled ghost 中性色与 Text hover 颜色，并重新验证全部颜色/变体矩阵。

日志：`out/044-integration-final.log`（完整 headless）、`out/044-integration-native.log`（原生构建的共同回归）。真实 Windows GPU/窗口验收属于任务 6.1，Linux native 单独 pending。
