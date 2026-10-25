# 084 父分支集成

父基线 `33591b945b72dc1ff3f16460245c1294a20068e4`，显式合入 `54aef7bc08a79d43c809e3f47bc9dfdbd26e01c0`，无冲突。该候选是原 f3a900ec 的纯后代，包含全部六项 R1 修复；没有导入未发布 Message/Notification 或 Timeline。

## 审查与身份边界

独立 Astra R2 报告 **CLEAN，未发现新的可操作 P1/P2**；六项字号原子性、双向同帧定位、子 trigger Escape、真实首焦点、延时更新和即时 focus 问题均关闭，声明首版范围未发现额外缺口。审查直接运行 worker 已有 Debug/Release 各 **17/17**，没有重建或重跑完整 44×2。

R2 没有自行使用 Git 核验提交身份、完整历史 diff，旧 golden/IIFE/栈结论沿用 worker 记录。父在合并前实际读取 worker HEAD 为上述完整 SHA、状态干净，并从 Git 对象核对直接父 `3573388d`，再合入该精确对象；不把 R2 的有限执行夸大为父 main 验收。

原 f3 候选曾在 ROOT 通过 39/39，但随后 R1 发现六项 P2，父已保存草稿并撤销当时的待提交 merge，**未发布 f3**。该历史结果不替代本轮修复后的新构建。

## 范围

父仅补充 README、Popover partial overlay、生成 catalog 与完成/验收记录。原有 086 Gallery 的 358 IDs 和实际详情/加载流程应保持。窗口内嵌原生控件继续使用自身 Theme；首版不含 contextMenu、动画、RTL、内置内容滚动、OS AT。

本次使用 ROOT 正式 `windows-msvc-debug`、MSVC x64 / Ninja Multi-Config，最多4 jobs；新的组件/Theme/共享回归与 inventory 结果单独记录。完整 Gallery 和 Windows 启动留待已冻结的 084/085/086/087 范围全部合齐后统一执行；不新增功能任务，不冒充 GPU/人工视觉、Linux、系统 IME 或 AT 验收。

## ROOT 修复版实际验证

`out/popover-parent-msvc.ps1 -Stage <build|tests|inventory> -RunId r2`：

- build：正式 MSVC Debug 新构建 **25** 个目标，**159** 个 Ninja 构建动作，退出 0。包含新 Popover review、Time component/review 以及原组件/Theme/Gallery 目标。
- tests：Popover 20 项、Date 六项、Time 九项、Theme 两项、七个共享组件、八个目录/source contracts，合计 **52/52**，real **11.21 s**，退出 0。六项新回归均使用本轮新构建的 ROOT 程序执行。
- inventory：新 frame `--inventory-only` 退出 0；**358 IDs、93 navigation、367 Buttons、1412 interactions、671 visible、151 Inputs、291 live**。Descriptions1/Skeleton4 以及旧 Form3/Item6、Date2/Time2、Breadcrumb2/Steps1 保持。
- `compare-theme.py`：五份 JSON 的 **15,372** 个旧 typed leaf、**33** 个旧 IIFE 原文/顺序及旧 TokenIdentity 保留；新构建的 Theme algorithm 测试实际解析与提交 goldens 一致。
- clang-format 22.1.3：**690** 个文件零失败；OpenSpec 1.14.0 doctor healthy、strict **85/85**，diff checks 通过。生产源码逐文件与被审 `54aef7bc` 一致，086 生产样例、frame tests 和 anchor 与父 `33591b94` 一致。

原始日志 `out/084-main-r2-{build,ctest,inventory}.log`，JUnit `out/084-main-r2-ctest.xml`。原有四条 frame MSVC warnings 和 VSDevShell 的 vswhere 提示保留。未重复 worker 的 44×2 或约1001秒完整 Gallery；这不替代后续统一全套与独立 Windows 启动证据。

仅推送 main，推送后核对远端 SHA；不推送 `t3/*`、不创建 PR/archive，原未跟踪 JPEG 保留。

## 后续统一验收

与 085/086/087 合并后的冻结源码 `fbce29a0` 已完成完整 Gallery 9/9 和独立 D3D12/DXIL 七帧启动，见 [批次最终记录](../../086-20261025-add-descriptions-and-skeleton-gallery/evidence/03-frozen-batch-final.md)。这不扩大 Popover 组件的 GPU/人工视觉、Linux、系统 IME/AT 或交互 Gallery 验收范围。
