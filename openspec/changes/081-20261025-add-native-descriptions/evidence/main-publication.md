# 081 收尾集成与发布

2026-10-25，父基线为已发布 Skeleton 的 `d74f6c6c3d2ae954c077834b563c2e2bb3acedc3`；通过显式 merge 合入 Descriptions 候选 `82faa8815feaf6512c177008263ab5cf94489457`，保留双方提交历史。候选的 R2 修复与修复前后实跑证据见 [selection decoration](r2-selection-decoration.md)。

## 合并保留

冲突均为两个新增组件的并行 Theme/CMake 接入及五份 golden。逐项保留两套公开 token、配置、accessor、snapshot 字段、解析、序列化/hash/equality、dirty 订阅和 source 注册，没有用单方整个文件覆盖另一组件。

- 保留已发布的 Skeleton TokenIdentity ordinal，Descriptions 的 colors/metrics 两项追加在其后。
- 两个直接父基线各自的 **32** 个 IIFE 原文和相对顺序都保留，合并后 **33** 个。
- Descriptions host/model、selection 修复、NodeStore、Input、Button 和 retained surface 的生产源码逐文件与 `82faa881` 相等。
- Skeleton API/model/host/Theme 源码、既有 frame tests、Gallery definition/runtime/viewport 逐文件与 `d74f6c6` 相等。目录只更新 Descriptions 的 partial 范围并重新生成 catalog；没有新增样例或更改已有业务流程。
- 中英文 README 与 completion 增加真实组件入口。静态 default config、heap descendant preflight、self-config guard、binding bridge 与公共 renderer ABI 不变。

## 独立 Astra R3

全新 Astra xhigh/default 对固定 worker `82faa881` 只读复核，结论 **CLEAN，未发现可操作的新 P1/P2**。selection focus/shadow 使用 decoration clip，fill/border/wave/compact seam/hit 保持 content 路径；四类 selection 控件在普通/Modal 中的实际 ring coverage 均为 `1 → 0 → 1`。原 R1 文字分区与 Button/Input 断言未弱化。

本轮审查直接运行 worker 已有 Debug/Release 二进制各 **11/11**（region 七项与 portable selection/switch/radio/modal 四项）。没有重建，也未读取 ROOT/out；该复验不替代下述父分支实际新构建。

## ROOT 实际新构建与失败处理

正式 `windows-msvc-debug`，MSVC x64 / Ninja Multi-Config，最多 **4 jobs**。`out/wrapup-msvc.ps1 -Phase descriptions` 对 Gallery/frame/catalog、Descriptions 七个目标、Skeleton 五个目标、Theme algorithm/runtime 与七个相关 native control 测试目标实际新构建，共 24 个目标。

首次 CTest 为 **43/44**；`switch_features` 崩溃。CDB 用 `sxe sov; sxe av; g; k 30; q` 实测为 `0xC00000FD` 栈溢出，调用链经过 `theme_and_handle_feedback → Signal<ThemeConfig>::set → ThemeScope::update → resolve_theme → derive_button`。MSVC Debug 为测试同一函数里多次大型按值 ThemeConfig 参数预留了叠加栈空间。

仅在 `tests/switch_features_tests.cpp` 增加小型 `publish_theme` 函数承载原 `Signal<ThemeConfig>::set` 调用，原配置、断言和公共 API 行为全部保留；没有修改生产逻辑来绕过失败，没有扩展 linker stack。重新编译该测试后，PE 的 stack reserve 仍为 **1 MiB**。

最终实际执行：

- `cmake --build --preset windows-msvc-debug --parallel 4 --target ...`：上述 24 目标成功；最后一次增量实际重编/链接 Switch fixture。
- 新 `rynui_theme_algorithm_tests.exe --write-goldens` 生成五份 canonical goldens；与 `d74f6c6`（仅新增 descriptions）及 `82faa881`（仅新增 skeleton）双向深比较通过，两方旧字段、类型和值均保持，identity 随新增组件正常变化。
- 精确 CTest：Descriptions **16**、Skeleton **11**、Theme **2**、Button/Input/Selection/Switch/Select/Modal/Feedback **7**、目录/source contracts **8**，合计 **44/44**，real **6.34 s**，退出 0。
- 新 frame binary `--inventory-only` 退出 0：**334 IDs、93 navigation、350 Buttons、1380 interactions、647 visible、147 Inputs、286 live**；Form3/Item6、Date2/Time2、Breadcrumb2/Steps1 保持。无需重复固定 079 源码的 1001 秒完整 frame。
- clang-format 22.1.3：**674** 个文件零失败；doctor healthy、strict **83/83**，diff checks 通过。

最终日志位于 `out/wrapup-descriptions-{build,ctest,inventory}.log`、`out/wrapup-descriptions-ctest.xml`；崩溃定位记录为 `out/wrapup-switch-cdb.log`。正常构建仍保留原有 frame warnings 和 VSDevShell vswhere 提示。worker 原有 Debug/Release 及 Astra 复验结果分别记录，未冒充 ROOT 新 Release 验收。

## 验收边界

本轮只做收尾集成，不新增动态拓扑、自动断点、RTL、交互 Gallery 或平台能力。此前 079 在 `22ee615` 的完整 frame **9/9** 与 D3D12/DXIL 原生启动不作为新 Descriptions/Skeleton GPU 验收；人工视觉、GPU/DPI、Linux、系统 IME、AT 仍需独立证据。不推送 `t3/*`，不创建 PR 或 archive。
