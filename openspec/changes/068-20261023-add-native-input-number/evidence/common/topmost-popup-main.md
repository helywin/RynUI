# 顶层 popup 滚轮集成（2026-10-24）

在 main `d837c35119f643b7a96cac14ea429df478a5be67` 合并 `38ad2d2b1cff9d4c915385dd3ebc56a46d416a9f`。Select 的滚轮归属取实际 top hit 的最近 popup layer，不再仅按自身 popup 矩形判断；保留 Dropdown 边界阻挡、子 Select 优先、同级 Select/Dropdown 的可见顺序，以及 Collapse 未聚焦 popup cleanup。

本机 Windows MSVC `windows-msvc-headless` Debug **8/8**（15.15 秒）、Release **8/8**（8.11 秒），重新构建后执行 Dropdown、Select、Modal、Table、Collapse、InputNumber component、Switch features 与 Theme algorithm。上游四个新复现（短菜单、长菜单边界、padding、同级 Select）及既有 Table/子 popup 场景均包含在 Dropdown tests 中。

父任务只格式检查本次改动的 Select/Dropdown 测试源码，避免与并行 Gallery 编辑互相改写；doctor、strict validation 与 diff check 通过。输出为 ignored `out/popup-wheel-r2-root-{debug,release}-{build,ctest}.log`。独立 Astra R3 正在进行，此记录不提前宣称审查结论；没有执行原生 GPU、Linux 或系统 IME 验收。

## R3 子 popup 内容优先

R3 确认 Select 的 closest-popup 修复本身无新问题，但发现 Dropdown fallback 会在子 Popconfirm 内 Table 之前消费滚轮。合并 `63c4fa723b7d62b2103a450bf5c6ab92d77f09c3` 到 `9ecb1f583847ff2674de9d003ad721d89461d3a9`：先运行既有内容 scroll handlers，再运行 Dropdown 边界兜底；Table 沿实际 hit 的祖先链查找滚动 owner，并在窗口层边界停止；Modal 仅处理自己的实际 overlay layer。保留全部 hosts 及定位顺序。

新构建的本机 MSVC headless Debug **11/11**（17.97 秒）、Release **11/11**（9.41 秒）：Dropdown、Select、Menu、Modal、Table、Collapse、Segmented、InputNumber、Feedback、Theme algorithm 与 Switch features。八个旧复现和三个新嵌套 popup 场景通过；原始输出为 ignored `out/popup-wheel-r3-root-{debug,release}-{build,ctest}.log`。Astra R4 独立复核进行中，没有预先宣称结果，未补充原生平台验收。

## R4 空 Table 的背景命中

R4 发现默认空 Table 正文没有 interaction，最近命中 owner 检查导致横向溢出列无法滚动。合并 `585fa7a61ec3488c1bf9fa2584c1776d477cc7e5` 到 main `7777da60544040973cef1e3a7287bc5133f6b191`，为正文添加子内容之前的背景 interaction，沿用节点的平移/裁剪与 HitTest 更新，不可聚焦且不增加 Tab stop；销毁清理 pointer、interaction、fragment 与 layout。

本机重新构建执行同一组十一项：Debug **11/11**（6.06 秒）、Release **11/11**（3.14 秒）。实际空表格横向偏移 **0 → 32**，保留既有十一项 wheel 回归，并覆盖背景 identity、子内容优先、裁剪、零高度 eligibility、Tab 与清理。原始输出为 ignored `out/popup-wheel-r4-{debug,release}.log`。首次构建命令误用了不存在的 portable `_tests` target，未运行测试；更正为实际 target 后重新构建并通过上述结果。

Astra R5 在 worker `585fa7a` 对 `63c4fa7..585fa7a` 独立审查，确认 R4 修复且无新增 P1/P2，直接运行既有 Debug/Release 定向二进制各 11/11 通过。popup 内空 Table 的额外组合仅做源码复核，没有新增独立执行证据。本次没有原生 GPU、Linux 或系统 IME 验收。
