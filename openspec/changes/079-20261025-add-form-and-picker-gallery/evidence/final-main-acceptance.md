# 079 完整 main 验收

## 固定源码与实际环境

2026-10-25，在 `D:/code/RynUI` 的已推送 main **`22ee615347967dc204d89ff3508b74e63c68ad5e`** 上完成实际验收。构建、CTest、原生启动前后共十一处 guard 均确认相同 HEAD、main 分支和 tracked clean；三个 executable 的构建后 SHA-256 在各阶段与最终检查保持。原未跟踪截图保持，未读取或改写其内容。测试子任务没有修改源码、tasks 或 Git；父线程收到完整结果并核对 manifest/JUnit 后才解除源码锁。

环境为 Windows / MSVC / Ninja Multi-Config，官方 configure preset `windows-msvc`、build/test preset `windows-msvc-debug`，并行度 4。实际编译 definition、anchor、frame TU 并重新链接三个目标，**21.049 秒，exit 0**：

```text
cmake --build --preset windows-msvc-debug --parallel 4 --target rynui_token_gallery rynui_token_gallery_frame_tests rynui_ant_design_reference_catalog_tests
```

## 一次完整 frame 与八项 contracts

```text
ctest --preset windows-msvc-debug --parallel 4 --output-on-failure --output-junit D:/code/RynUI/out/079-final-root-20261025-22ee6153-1792880831971-ctest-junit.xml -R "^rynui\.(token_gallery_frame|ant_design_gallery_catalog_generator|ant_design_gallery_catalog_contract|ant_design_reference_catalog|gallery_document_contract|gallery_document_viewport_contract|token_gallery_contract|token_gallery_evidence_schema|windows_gallery_acceptance_runner_contract)$"
```

精确 **9/9 PASS**，CTest exit **0**；完整 `rynui.token_gallery_frame` **1001.43 秒**，CTest real **1001.45 秒**。八项目录/源码/证据 contracts 同轮通过。没有执行指向同一无参数程序的重复 journey 注册，也没有再次运行 scoped flags 或未改变的 headless 全矩阵。JUnit tests=9、failures=0；所有源版本 guard 与二进制身份检查通过。

full-frame 断言与 native telemetry 一致：**334 IDs、93 navigation、350 Buttons、1380 interactions（647 visible）、147 Inputs、286 live samples**；其余包括 22 Select、7 Card、14 Tabs、13 Modal/Drawer、10 OTP groups、46 selection、2 Checkbox groups、4 Radio groups、4 Typography、11 Divider、1 Table、1 Pagination、2 Menu、1 Dropdown、1 Collapse、3 Segmented、3 Empty、3 Result、67 Theme content、6 document sections、73 reference entries、126 reference surfaces。样例源码声明构成为 **3 Form / 6 FormItem、2 DatePicker、2 TimePicker、2 Breadcrumb、1 Steps**；没有为了盘点再单独运行一次 inventory。

## Windows 原生启动

对本轮新构建程序实际执行：

```text
out/build/windows-msvc/examples/Debug/rynui_token_gallery.exe --smoke
```

正常退出，**45.749 秒**，实际输出 **`gpu_driver=direct3d12`、`shader_format=DXIL`、`frame_samples=7`、`exit_code=0`**，stderr 为空。没有从历史日志推断 driver，也没有切换驱动或安装工具。四条 MSVC warning 与已知 VSDevShell `vswhere.exe` 提示保留在原始日志，构建及程序实际退出码均为 0。

## 独立审查与证据

Time 阶段全新 Astra R4 对 `6ceff51b` 的完整 079 三阶段及 082 共存结论 CLEAN；实际运行 time-picker、inventory、states，核对其余阶段记录、源/obj/exe/Ninja 与历史精度、Collapse ownership、Date/Time 无 blur reset、导航 fixture，见 [stage3-time-gallery.md](stage3-time-gallery.md)。`22ee615` 只在该 reviewed source 上增加父记录与完成清单，本轮测试源码与所审代码一致。此前 Form/Date/082 和 Core Time/Nav 的各轮审查记录继续保留，不以本轮运行冒充各轮独立重建。

本机 ignored 原始产物均以前缀 `out/079-final-root-20261025-22ee6153-1792880831971-` 保存：`summary.log`、`manifest.json`、`ctest-junit.xml`、`ctest-LastTest.log`、`{build,ctest,smoke}-{stdout,stderr}.log`；manifest 包含精确命令、逐项秒数、所有源码 guard、三个 executable 的哈希/时间戳及 telemetry。helper 为 `out/079-final-msvc-helper.ps1`。这些机器产物未混入 Git；本记录保留可复现命令、固定源码和准确汇总。

079 **4/4**：Form、DatePicker、TimePicker 三阶段和最终统一验收完成。父收口只修改记录/任务/清单，核对实现相对验收 SHA 不变后按英文 Conventional Commit 提交，安全 fetch、推送 main 并比较远端 SHA。不推送 t3 分支，不创建 PR 或 archive。

本次 Recording 加原生启动确认实际对应的运行范围；组件人工视觉、完整组件 GPU/DPI 矩阵、Linux、系统 IME 与辅助技术仍按各自证据独立验收，不能由此记录代替。
