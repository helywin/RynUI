# TimePicker main 交付

## 经过独立审查的合并

全新 Astra xhigh R4 固定审查 `a671835218a5f134ed211f4b96e331be79cf04ff`，双亲为 Time R3 CLEAN 的 `1116a9931d1c6ea07b25739319570bfd092f0bc3` 和已发布导航/Date/Form 的 `c083fed5a72f644b491118c6440908e0354d241e`。结论 R4 CLEAN、未发现此次共享合并引入的可行动 P1/P2。main 121 个指定文件、Time/Input 17 个文件原文及对应 Git blobs、唯一 reset hook、R1/R2 回归、Nav lifetime、Date/Form reset 与 Gallery/catalog/README 均保持。

R4 独立运行已有 worker 程序，Debug **28/28**、Release **28/28**，每项每配置一次；包含六组 Time review 的十三个行焦点场景、共存例、Time/Nav Theme、两个 Nav component、五组 Nav review、两组各八轮 lifetime、Date reset、Form、Theme 四程序及 Focus 三程序。三十六个二进制前后哈希不变，PE 栈保留均为 1 MiB。这是既有二进制复验，不是本轮独立重建；worker Debug build 日志中的 `ninja: no work to do` 结合已有 Ninja 编译/链接记录、源码→对象→程序时间顺序读取。

worker 真实去重组合 Debug **44/44，38.66 秒**、Release **44/44，15.85 秒**，三十二个目标及 fresh goldens 证据见 [nav-merge.md](nav-merge.md)。五份 fresh golden 保留 main **14,632**、Time **14,447** 个旧 typed leaves；三十个 main IIFE 原文/顺序及 Time 独立块保持，Time TokenIdentity 追加在已发布 Breadcrumb/Steps 之后。静态默认配置、heap descendant preflight、self-config guard、binding bridge 和内容先行/Time→Date→Dropdown boundary 相对顺序均保留。

## 父树实际集成

父树从已发布 082 导航 Gallery 的 main `0ef766e07d0cf79efbad010a015fc924a6422754` 执行 `git merge --no-ff --no-commit a671835`，自动合并无冲突。`git diff --quiet a671835 -- src include tests/CMakeLists.txt tests/time_picker_review_tests.cpp design-tokens` 退出 0；已发布 examples、Gallery、frame tests 与 Breadcrumb/Steps 文档相对 `0ef766e` 原样保持。未导入 Descriptions WIP，原未跟踪截图保留。

TimePicker 的 partial 支持范围、README API 入口及组件清单按已实现的严格二十四小时单时间、保留列、确认/即时提交、规则、controlled authority、String Form reset 和 Theme 更新；交互 TimePicker Gallery、复杂格式/范围及平台缺口仍保留。目录由合并后 overlay 重新生成，保留已发布 Breadcrumb/Steps Gallery 元数据。

父树官方 Windows / MSVC / Ninja Multi-Config `windows-msvc-debug` 实际重建 `rynui_token_gallery`、`rynui_token_gallery_frame_tests`、`rynui_ant_design_reference_catalog_tests`，退出 0，日志 `out/time-picker-final-main-native-build.log`。重建 frame 的 `--inventory-only` 退出 0，**319 IDs、93 navigation、337 Buttons、1072 interactions（634 visible）、284 live、144 Inputs、2 Forms/5 items、2 DatePickers、2 Breadcrumbs、1 Steps** 保持，日志 `out/time-picker-final-main-inventory.log`。Time 交互合同证据来自上述 worker 组合与独立复验，父 inventory 验证既有 Gallery 共存。

八项 catalog/document/evidence contracts **8/8，0.20 秒**，日志 `out/time-picker-final-main-catalog.log`。父深比较五份 goldens 相对 `0ef766e` 仅新增 timePicker、旧值/类型和三十段 IIFE 保留；文件与已审查 `a671835` 全字节一致。父没有再次生成旧 headless binary 的 goldens。clang-format **22.1.3** 检查 **646** 个源码零失败，doctor healthy、strict **81/81**、generator 与 diff checks 通过。没有重复未改变的平台通用矩阵。

## 边界与交付

父线程只提交推送 main，安全 fetch 后核对 `git ls-remote origin refs/heads/main` 与本地 HEAD，结果记录在交付消息；不推送任何 `t3/*`，不创建 PR/archive。079 的 TimePicker Gallery 和完整 frame/native smoke 仍为后续独立任务。本轮本地构建、Recording/contract 或已有程序复验不扩展为组件 GPU、原生窗口人工视觉、Linux、系统 IME 或辅助技术完成。
