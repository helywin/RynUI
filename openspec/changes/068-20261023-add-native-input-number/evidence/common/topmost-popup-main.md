# 顶层 popup 滚轮集成（2026-10-24）

在 main `d837c35119f643b7a96cac14ea429df478a5be67` 合并 `38ad2d2b1cff9d4c915385dd3ebc56a46d416a9f`。Select 的滚轮归属取实际 top hit 的最近 popup layer，不再仅按自身 popup 矩形判断；保留 Dropdown 边界阻挡、子 Select 优先、同级 Select/Dropdown 的可见顺序，以及 Collapse 未聚焦 popup cleanup。

本机 Windows MSVC `windows-msvc-headless` Debug **8/8**（15.15 秒）、Release **8/8**（8.11 秒），重新构建后执行 Dropdown、Select、Modal、Table、Collapse、InputNumber component、Switch features 与 Theme algorithm。上游四个新复现（短菜单、长菜单边界、padding、同级 Select）及既有 Table/子 popup 场景均包含在 Dropdown tests 中。

父任务只格式检查本次改动的 Select/Dropdown 测试源码，避免与并行 Gallery 编辑互相改写；doctor、strict validation 与 diff check 通过。输出为 ignored `out/popup-wheel-r2-root-{debug,release}-{build,ctest}.log`。独立 Astra R3 正在进行，此记录不提前宣称审查结论；没有执行原生 GPU、Linux 或系统 IME 验收。
