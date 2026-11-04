# Layout / Splitter 主线集成

2026-11-04，Linux x86_64 / GCC 16.2.1，`linux-gcc-headless` Debug / Release，Ninja Multi-Config，构建最多 `-j2`。

- 顺序应用本地 Layout `9627c13f`、Splitter `9ba4aa42`；共享 Theme/服务/目录冲突作为同一阶段解决和验证。保留 FloatButton、typed Typography rich runs/双 MIME、Grid/Masonry、共享窗口逻辑宽度断点；三个 helper 与集成前逐字相同，CMake 与服务注册各一次。保留有限原生验收的 Wayland frame callback 同步。
- 构建 Shell/Splitter、两者独立 Gallery、Grid/Masonry、Theme algorithm/runtime/scope/public API、目录和页面模型相关目标。定向 CTest **Debug 54 项通过、Release 54/54 通过**；Debug 首轮 53/54，仅 source inventory 仍为旧 70，按真实目录改为 **77** 并补 Layout/Splitter API 断言，定向复验 **1/1**。未重跑全量 CTest。
- 覆盖 Shell 11 项、Splitter 16 项、Grid 7 项、Masonry 9 项；Theme/目录/生成/页面/依赖守卫共 11 项。原分支细节矩阵见 [Layout](layout-linux-headless.md) 与 [Splitter](splitter-linux-headless.md)，本集成不将 HEADLESS 计为真实窗口验收。
- 五份 Theme golden 在写入前通过结构化 JSON 与集成前 `d7600ec2` 比较：所有既有字段除 identity 外完全相同，仅新增 `floatButton`、`layout`、`splitter`。恢复完整 diagnostic 字节相等检查，保留 FloatButton 默认值/identity 独立断言。golden 是测试实际读取的确定性输入。
- 目录为 **73 项 / 7 类**，implemented **17**、partial **38**、planned **17**、deprecated **1**；General **4/4**、Layout **7/7** 原生合同实现。真实源码示例 **77**，冻结 legacy inventory **458** 与基线逐字不变。DOM/CSS/React 专属接口继续排除。
- 10 个公开头独立 C++20 `g++ -fsyntax-only -I include` 通过；HEADLESS backend 边界含 include/link 正反向守卫通过。clang-format 22.1.3：**876 文件 / 0 failures**；目录 `--check --self-test`、OpenSpec doctor、strict **106/106**、`git diff --check` 通过。

本阶段完成 4.1/4.2/6.1/6.2，任务 **12/16**；Linux 7.1/7.2 等待后续真实后台验收与最终 Release 构建，Windows 8.1/8.2 未测且保持未勾选。未操作用户桌面或系统鼠标键盘。构建/原始日志与诊断只在 ignored `out/106-final/`、`out/build/`；无 PR、无 archive。

定向 CTest 过滤器（Debug / Release 相同）：

```text
^rynui[.](shell_layout_|splitter_|grid_|masonry_|theme_algorithm$|portable[.](theme_runtime|theme_scope_component|theme_public_api|backend_boundaries)$|ant_design_reference_catalog$|gallery_page_model$|ant_design_gallery_catalog_(generator|contract)$|gallery_sample_source_(contract|generated)$)
```
