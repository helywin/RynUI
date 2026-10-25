# 084–087 冻结批次最终验收

2026-10-25，ROOT `D:/code/RynUI`。用户要求完成现有四项后停止，未新开功能/change、PR 或 archive。最终功能源码固定为已推送的 **`fbce29a0df44347f90a0a2081d57026a80fa4307`**，验收期间 main/HEAD/tracked clean 的九次启动前后 guard 全部通过。

## 完成范围与审查

- **084 Popover**：typed 富内容、controlled/default、组合触发与延时、十二方位/箭头、真实原生输入及子浮层归属。六项组件 R1 修复后独立 Astra R2 CLEAN；父已发布 `b5728c17`。
- **085 Message/Notification**：窗口 holder/ref、keyed 保留更新与关闭、五种状态、四角通知、数量限制与单调自动关闭。重入淘汰、普通 notice 填充裁剪、最后 notice 关闭后源 draw 恢复等修复经独立 Astra R3 CLEAN；父已发布 `ddb8a120`。
- **086 Gallery**：Descriptions 实际资料编辑/保存、布局与恢复，Skeleton 实际加载/发布、草稿与 refs 保留，生产目录直达；三主题双有限视口与独立 Astra R1 CLEAN，父已发布 `33591b94`。
- **087 Timeline**：固定 keyed 竖向富条目、Start/End/Alternate、reverse/visible、静态 loading 与分批虚线。组件 R2 CLEAN；与 notices/Popover 合并时新增的 document hit 问题，在 Debug/Release 先失败后修复，组合 Astra R2 CLEAN。父新构建 Debug **96/96**、MSVC headless Release **53/53**，最终发布 `fbce29a0`。

各组件首版范围和排除项仍以 API 文档为准。Astra 的现有程序复验、纯源码组合审查与父实际新构建分别记录，未互相替代。共享 clip、Theme、ordinal 保留性和实际命令见 084/085/087 的 `evidence/main-integration.md`。

## 固定源码上的精确执行

正式 `windows-msvc-debug`，MSVC x64、Ninja Multi-Config、最多 4 jobs。辅助脚本 `out/084-087-final-msvc-helper.ps1` 的 RunId 为 `20261025-fbce29a0`，三个阶段依次执行，均以完整 `ExpectedSha` guard：

```powershell
./out/084-087-final-msvc-helper.ps1 -Stage build -RunId 20261025-fbce29a0 -ExpectedSha fbce29a0df44347f90a0a2081d57026a80fa4307
./out/084-087-final-msvc-helper.ps1 -Stage ctest -RunId 20261025-fbce29a0 -ExpectedSha fbce29a0df44347f90a0a2081d57026a80fa4307
./out/084-087-final-msvc-helper.ps1 -Stage smoke -RunId 20261025-fbce29a0 -ExpectedSha fbce29a0df44347f90a0a2081d57026a80fa4307
```

build 对应 `cmake --build --preset windows-msvc-debug --parallel 4 --target rynui_token_gallery rynui_token_gallery_frame_tests rynui_ant_design_reference_catalog_tests`，exit **0，0.8409862 秒**。三个程序在 087 父阶段已实际重新编译/链接；冻结后的本次构建仅部署/同步 shader，没有再次编译 C++，不把这一检查称为三 TU 新编译。

CTest 使用精确锚定九项名称的正则，`--output-on-failure --parallel 4 --output-junit`；完整无参 frame **只执行一次**，没有选重复的 `gallery_platform_generic_journey`，也没有重跑全部 scoped flags。

| 实际测试 | 秒 | 结果 |
| --- | ---: | --- |
| rynui.token_gallery_frame | 1800.51 | PASS |
| rynui.ant_design_gallery_catalog_generator | 0.150787 | PASS |
| rynui.ant_design_gallery_catalog_contract | 0.163549 | PASS |
| rynui.ant_design_reference_catalog | 0.0110537 | PASS |
| rynui.gallery_document_contract | 0.0516355 | PASS |
| rynui.gallery_document_viewport_contract | 0.0327464 | PASS |
| rynui.token_gallery_contract | 0.0775349 | PASS |
| rynui.token_gallery_evidence_schema | 0.0388864 | PASS |
| rynui.windows_gallery_acceptance_runner_contract | 0.0358279 | PASS |

**9/9 PASS，CTest real 1800.53 秒，进程 exit 0**；外层计时 1800.5958403 秒。完整模式覆盖原有流程以及新增 Descriptions/Skeleton 的三主题双视口、真实事件、glyph/fill/hit/focus、草稿/preedit/popup/retained 检查，Compact 的实际 `clipped_fills=2` 断言保留。运行中做过一次 `cdb -pvr` 不暂停进程的只读采样，未可靠展开完整栈；没有重启、终止或重复执行验收。单次验收耗时不作为性能基准。

原生 `rynui_token_gallery.exe --smoke`：exit **0，68.8248021 秒**，stderr **0 bytes**；实际 telemetry：

```text
gpu_driver=direct3d12 shader_format=DXIL
window_system=win32 display_scale=1.5 host_display_scale=1.5
stable_test_ids=358 live_samples=291 interaction_count=1412
frame_samples=7 submits=7 exit_code=0
```

这是正常窗口/系统字体/真实 Direct3D 12 启动、呈现与退出证据。没有开启专属 `--*-acceptance`；telemetry 的 `header_navigation=failed` 是该独立 scrollbar 检查的初始未执行值，该项仅在 `scrollbar_acceptance` 时判定，不计为本次已验收通过。animation/input/selection acceptance 也未开启，不把这些默认值改写成通过。

Gallery 精确组成保持 358 IDs、93 navigation、367 Buttons、1412 interactions、671 visible、291 live、151 Inputs、24 Select、3 Forms/6 items、2 DatePicker、2 TimePicker、2 Breadcrumb、1 Steps、1 Descriptions、4 Skeleton；full-frame 合同保留对应断言。原生 telemetry 独立确认其中 358/291/1412，未额外重复 inventory 程序。

## 程序身份与原始记录

三个程序的 SHA-256、大小、mtime 在 build 后、CTest 前后、smoke 前后逐项一致：

| 程序 | SHA-256 | bytes | 最后修改 UTC |
| --- | --- | ---: | --- |
| rynui_token_gallery.exe | 32B290076D4D2EAC929D5F96B94E0238C5F12CC5FE88334B2F92B565CC083EC7 | 43352064 | 2026-10-25T07:43:21.0227878Z |
| rynui_token_gallery_frame_tests.exe | 2A19B9A48B6C849DC960EA3A48C65C55FA5036F26170BF3A0ABAADF5E964774B | 33980928 | 2026-10-25T07:43:52.7196669Z |
| rynui_ant_design_reference_catalog_tests.exe | 256A6839CAD788C23D09D6AC1235119A17472D416CD32AD43CC5ED9D4F0D9A31 | 893440 | 2026-10-25T07:43:50.4641753Z |

完整本地记录统一前缀为 `out/084-087-final-main-20261025-fbce29a0-`：`manifest.json`、`guards.log`、`ctest-junit.xml`、`ctest-LastTest.log`、各阶段 `*-stdout.log`/`*-stderr.log` 和只读采样日志。保留原 VSDevShell vswhere 提示和此前实际重建时的 MSVC warnings，不把启动脚本提示冒充测试失败或隐藏。

## 收尾边界

本轮平台通用 Recording/组合合同和 Windows 原生启动通过；**不代表所有新增组件的人工视觉、完整 GPU/DPI、Linux、系统 IME 或 AT 验收**。Popover、notices、Timeline 尚无本轮交互 Gallery，首版排除项未删除。没有修改 renderer ABI、扩大 PE 默认栈或以目录状态代替实现。

验收结束后仅更新 Markdown 记录；最终提交与被冻结源码在生产文件上保持一致。只推送 main 并核对远端 SHA，不推送 t3 分支；不创建 PR/archive。完成记录和推送后停止持续开发。
