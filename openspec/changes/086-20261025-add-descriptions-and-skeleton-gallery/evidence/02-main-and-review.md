# 086 父分支集成与审查

父基线 `bfbb49234c945c52988b8474838d3ac88bbc84ec`，显式合并候选 `84b65b7ba72512064226b846c63a83719ff05c64`，无冲突。Core/API/Theme/goldens/CMake 与父基线完全相同，Gallery 生产实现与被审候选逐文件一致。

## 独立审查

全新 Astra xhigh/default R1 固定审查上述 worker，结论 **CLEAN，未发现可操作的 P1/P2**。审查确认编辑与保存反馈一致，外部恢复控制有效，真实目录经生产 anchor 直达样例，草稿/preedit 清理和身份保留的断言有效；Compact 检查实际 ordered clipped fill、clip 和 coverage，没有放宽精确数量。

审查实际运行 worker 现有 Debug 程序：`--details-loading-only` 六组、`--inventory-only`、catalog、三个源码 contracts、基线比较、`--forms-only`，均退出 0。该结果为 existing-binary 复验，不冒充 ROOT 的新构建或统一全套。

## 保留与边界

父在添加自己的集成文档前运行原 `compare-baseline.py`：**334** 旧 IDs 顺序、**31** 旧样例函数体与调用顺序、**71** 非目标 overlay 逐项保持，Core/API/Theme/goldens/CMake 和原父元数据均保持；之后仅父 completion/本记录/父任务状态作为集成记录更新。

本阶段是平台通用 Recording 流程的新构建与 focused/inventory/contracts；完整 Gallery frame、Windows 原生启动 smoke 保留到 084/085/086 同批完成后统一执行，不能把本轮 focused 结果写成完整 frame、GPU、Linux、系统 IME 或 AT 验收。

## ROOT 本轮实际验收

正式 `windows-msvc-debug`，MSVC x64 / Ninja Multi-Config，最多 4 jobs；忽略目录脚本 `out/086-parent-msvc.ps1` 分阶段执行：

- `-Stage build -RunId r1`：实际重建 Gallery、frame、reference catalog 三个目标及其依赖，**112** 个 Ninja 构建动作，退出 0。此前未发布 Popover 候选的缓存已随当前源码重新配置和重建，未把它的二进制当成 086。
- `-Stage focused -RunId r1`：新 frame `--details-loading-only`，Default(step4)/Dark(step0)/Compact(step1) × 1200×900/560×900，**6/6**，退出 0。真实目录、编辑/选择/保存、布局/显隐、focus/glyph/surface/hit clip、loading/preedit/popup/refs/身份检查通过；Compact 两组实际 `clipped_fills=2`。
- `-Stage inventory -RunId r1`：退出 0；**358 IDs、93 navigation、367 Buttons、1412 interactions、671 visible、291 live、151 Inputs、24 Select、47 selection、Descriptions1、Skeleton4**。Form3/Item6、Date2/Time2、Breadcrumb2/Steps1 保持。
- `-Stage contracts -RunId r1`：精确八项 catalog/source/evidence contracts **8/8**，real **0.17 s**，退出 0。
- clang-format 22.1.3：**674** 个自有源码、0 failures；OpenSpec **1.14.0** doctor healthy、strict **84/84**，staged/unstaged diff checks 通过。

原始日志 `out/086-main-r1-{build,focused,inventory,contracts}.log`，JUnit `out/086-main-r1-contracts.xml`。构建中的原有四条 frame MSVC warnings 与 VSDevShell vswhere 提示保留。父实际运行不重复 worker 的全部旧长 flags；未改 Core 的旧代表由 worker 记录及 Astra 只读复验分别支持，不能冒充本轮 ROOT 重跑。

发布只推送 main，并在推送后核对远端 SHA；不推送 `t3/*`，不创建 PR/archive。原有未跟踪 JPEG 不读取、不暂存、不修改。

后续冻结批次的完整 Gallery 9/9 和独立 Windows 原生启动已完成，见 [实际最终验收](03-frozen-batch-final.md)；本文件此前的 focused/延期说明保持为阶段历史。
