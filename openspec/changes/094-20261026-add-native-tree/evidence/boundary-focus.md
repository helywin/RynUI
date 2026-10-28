# 边界行焦点修正（2026-10-28）

096 的独立 Astra R1 指出已有 Tree Core 首末行 outline 被根视口裁断。父在
`main@b1168746fe2929c6049e812907d007adbb39098f` 上单独补充 094 回归和修复；不把未完成 Gallery 改动并入本阶段提交。

## 真实复现和修复

新增 `rynui.tree_review_focus_clips` 在生产代码修改前实际编译运行：Debug exit 1，0.2107944 秒，
首行四边 coverage 为 `1,0,1,1`。这是实际 ordered effect、shader-reference coverage 和 ancestor clip 的交集，
不只是 focus_visible 或 wrapper containment。

按既有 `focus_width + focus_offset` 在 Tree 测量与放置中留出空间；横纵留白受实际约束夹紧，空树不增加 gap。
Home/End 显露使用扣除留白的内部区域，滚动范围包含留白。根视口及外部文档 clip 不放宽，
未修改 focus、input eligibility、hit、renderer、Theme、公开 API 或 token；已滚出视口的行不能向外绘制。

## 新构建与实际运行

Windows MSVC、官方 `windows-msvc-headless-{debug,release}`、Ninja Multi-Config、最多 2 jobs。
精确命令见 [driver](boundary-focus-artifacts/094-focus-msvc.ps1)，CTest 筛选为：

```text
^rynui\.(tree_.*|portable\.(layout_style|focus_order|focus_state|focus_lifecycle|modal_component))$
```

| 配置 | 最终 build | CTest | CTest real / wrapper |
| --- | --- | --- | --- |
| Debug | exit 0 / 10.2358161 秒 | 26/26 | 4.26 / 4.3592528 秒 |
| Release | exit 0 / 18.2052857 秒 | 26/26 | 2.11 / 2.206556 秒 |

新 case 每配置覆盖 Default/Dark/Compact × 180/56 宽、96 高，Home 首行、End 末行及裁剪恢复，
实际四边 coverage 均为 1；显式 document clip 可完全隐藏焦点，恢复后重新实际点击/按键可恢复。
行身份与实际 hit clip 保持。全部原 Tree 生命周期、零面积、checkbox translation、嵌套滚轮与 popup 隔离检查保留并通过。

Core TU 在修复后实际编译；最终 Debug build 还编译了后续回归夹具调整。相关源→OBJ→LIB→EXE 时序及
Ninja 编译/链接记录另存，不能仅将最后一次 build 日志解释为从零重建整个项目。
26 个 case 对应每配置 10 个 project EXE，共 20 个 PE，逐个实测默认 1 MiB stack。
Theme/golden/token/资源文件未修改，不伪称重新生成了金样。

## 夹具调整与失败保留

第一次修复后检查发现：旧 no-range nested Tree 固定为 24 高，已没有新自然尺寸所需的焦点空间；
改为 48 高并断言 `content_height <= viewport_height`，仍实际验证 inner=0、outer 前进及上下边界交接。
滚轮命中点移到行内部，避开新留白，原每次实际 inner ownership 断言、foreign owner 和 popup 边界断言均保留。

新 focus case 的外部裁剪检查起初要求已完全裁掉的 outline 仍出现在 ordered commands，后又固定裁剪高度 48，
遗漏窄行换行后的实际高度。现按当前行实际 bounds 与 outline extent 构造完全遮挡，允许绘制命令被正常剔除，
仍断言实际可见 coverage 全为 0；恢复使用显式输入，不要求自动复活旧焦点。
两份初始 after 失败日志与原 before 失败日志都保留，不覆盖为通过。

## 产物与边界

[逐字节索引](boundary-focus-artifacts/index.json) 包含原始日志、JUnit、两配置 CTest inventory、
[source/OBJ/LIB/EXE 身份](boundary-focus-artifacts/identities.json) 和过滤出的原始 Ninja 记录。
提交前对照 staged blob 验证 hash；UTF-16 日志不转码。format22、显式 OpenSpec1.14 doctor/strict 和 diff 检查按本阶段执行。
只推 main 并核远端 SHA；不包含 096 未验证的 Gallery，不创建 PR/archive。
结果仅为平台通用 HEADLESS；不代表 native/GPU、Linux、系统 IME 或 AT。
