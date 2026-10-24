# main 集成与独立 R3

2026-10-25，从已推送 main `62c990a1b494532c358ac487b2bb5931c08a7231` 执行 `git merge --no-ff --no-commit 7db4123842d9815adef1c86ce32e8a003c24e827`，仅 tests/CMakeLists 自动合并，无冲突。Breadcrumb/Steps 实现及回归与已审查 worker 相同；父 Date/Input reset 源码、Date review、Gallery definition/frame tests 与 `62c990a` 保持。父树未导入 TimePicker 或 Descriptions WIP，保留原未跟踪截图。

## 独立审查与 worker 验收

全新 Astra xhigh R3 通读完整 080 与历史发现，结论 CLEAN、无新的可行动 P1/P2；R1 富子控件输入与焦点轮廓、R2 input block 注销均已解决。scope 先停用，resource cleanup 先于状态/节点/ID 释放；按完整 generational 行 ID 仅移除自己的 block。独立运行现有 worker Debug **33/33**、Release **33/33**，新用例共 160 次同窗口循环，每轮 mounted/records/blocks 回到基线；文档示例检查通过。这是既有二进制复验，没有独立重建。

worker 实际构建、两配置修复前 10/10 失败与修复后 10/10、既有 24/24、fresh goldens 和工具检查见 [08-astra-r2-affected-validation.md](08-astra-r2-affected-validation.md)。父集成继续采用已完成的 worker Release 平台通用证据，另对新合并组合做以下实际验证。

## 父树实际验证

官方 Windows / MSVC / Ninja Multi-Config：

- `windows-msvc-headless-debug` 重新构建导航 model/examples、两组件/Theme、R1/R2 review targets 与受影响 Date/Form/Feedback/Theme/Button/Focus/Modal targets，定向 CTest **36/36，3.24 秒**，包含新增 Date focused form_reset 与 Core/backend 守卫。ignored helper `out/navigation-main.ps1`、日志 `out/navigation-main-debug.log` 保存精确目标/filter。
- 新构建 Debug Theme algorithm 执行 `--write-goldens`；五份金样与 `7db4123` 全字节一致，深比较 `62c990a` 全部旧类型/值（仅允许 global identity 与新增 breadcrumb/steps），原 **28** 个 IIFE 原文/顺序保留。日志 `out/navigation-main-goldens.log`；比较 helper `out/check-date-picker-integration.py 62c990a breadcrumb,steps`。
- `windows-msvc-debug` 重新构建 `rynui_token_gallery_frame_tests` 和 `rynui_ant_design_reference_catalog_tests`。实际 `--date-picker-only` 退出 **0，177.39 秒**：真实目录/月历/ISO/限制/清空/String Form、Default/Dark/Compact × 1200/560 × 900 的 glyph/surface/hit clip、无 blur 同值 reset 与焦点/开合/身份保持均通过；`--inventory-only` 退出 **0**。日志 `out/navigation-main-gallery-build.log`、`out/navigation-main-date-gallery.log`、`out/navigation-main-inventory.log`。
- 重建后八项目录/文档/证据 contracts **8/8，0.17 秒**，日志 `out/navigation-main-catalog.log`。filter 是 `^rynui\.(ant_design_gallery_catalog_generator|ant_design_gallery_catalog_contract|ant_design_reference_catalog|gallery_document_contract|gallery_document_viewport_contract|token_gallery_contract|token_gallery_evidence_schema|windows_gallery_acceptance_runner_contract)$`。

实际 inventory 保持 **297 IDs、93 navigation、320 Buttons、1044 interactions（606 visible）、282 live samples、67 Theme content、141 Inputs、22 Select、2 Forms/5 items、2 DatePickers**。本阶段只增加组件 API 和准确的 partial 目录状态，未添加 Breadcrumb/Steps live samples；交互 Gallery 由 082 独立推进。

clang-format **22.1.3** 检查 **631** 个自有源码，0 failures；doctor healthy，strict **80/80**，worktree/cached diff checks 通过。更新 Breadcrumb/Steps 的 supported/missing scope、README API 入口和组件清单，生成目录保持锁定的 Ant Design 6.6.5 baseline。

## 交付边界

父树只提交并推送 main，安全 fetch 后核对 `git ls-remote origin refs/heads/main` 与本地 HEAD；SHA 核对结果在交付消息记录。不推送 t3 分支，不创建 PR/archive。Recording 和 contracts 不代表组件 GPU、真实窗口人工视觉、Linux、系统 IME 或辅助技术完成；完整 Gallery/native smoke 仍在后续统一收口。
