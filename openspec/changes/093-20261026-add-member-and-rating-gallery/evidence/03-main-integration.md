# 093 父主分支集成与新构建验收

2026-10-28。父基线 `ee289de2ad6851ededdddd1c45bd20612b7579a8`，以无冲突、保留双亲的 merge 接入
`862ff37ce6799db344bd57e188068060ce15cd7b`。此记录属于 ROOT `D:/code/RynUI` 的新构建，
不把 worker 的旧二进制结果或原阶段冻结基线当作当前 main 验收。

## 当前状态

新流程、实测 inventory、八项合同与旧浮层/Timeline 代表全部通过；独立 Astra 组合源码审查 CLEAN。
本提交完成 task 2.1/3.1，并保留上述准确双亲；只发布 main，发布时核对远端 SHA，不推 t3 分支。

## 实际构建和运行

使用 Windows MSVC、官方 `windows-msvc` / `windows-msvc-debug` presets、Ninja Multi-Config，最多 2 jobs。
正式 configure 成功后，以下三个目标实际重新编译了组件、definition/anchor/frame、生成 shader、编译 RC 并最终链接，原构建 wrapper exit 0：

```text
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug --parallel 2 --target rynui_token_gallery rynui_token_gallery_frame_tests rynui_ant_design_reference_catalog_tests
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --members-metrics-only
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --inventory-only
ctest --preset windows-msvc-debug --parallel 2 --output-on-failure -R "^rynui\.(ant_design_gallery_catalog_generator|ant_design_gallery_catalog_contract|ant_design_reference_catalog|gallery_document_contract|gallery_document_viewport_contract|token_gallery_contract|token_gallery_evidence_schema|windows_gallery_acceptance_runner_contract)$" --output-junit <absolute out/093-main-contracts.xml>
out/build/windows-msvc/tests/Debug/rynui_token_gallery_frame_tests.exe --overlays-timeline-only
```

构建最初的命令 session 在上下文切换时没有保留；父线程打开原 build wrapper 的 Windows process handle，
等待其正常退出并读取 exit 0，没有重新启动或重复构建。原 Ninja 日志保留；不推算或伪造该次 build 总秒数。

| 新产物检查 | 结果 | 秒数 |
| --- | --- | --- |
| 成员/指标/评分 focused | 三主题 × 1200/560 六组全部通过，exit 0 | 373.9245216 |
| inventory | 所有断言通过，exit 0 | 26.0045987 |
| 八项目录合同，修复后 | 8/8，exit 0 | CTest real 0.25；wrapper 0.3473766 |
| 旧浮层/Timeline 代表 | 三主题双宽及实际业务断言全部通过，exit 0 | 176.1882969 |

六组 focused 实际检查真实目录操作、Avatar auto-fit 和 126 个 overflow 成员可达性、typed Statistic 精度和 loading、
Rate 半星/清空/键盘/富 Tooltip、实际 glyph/fill/hit、适用控件四边焦点 coverage、草稿/preedit 与完整 generation 保留。
纯展示 Avatar/Statistic 不伪造焦点语义；ref 方法的实际覆盖范围沿用原测试中的成员/指标 InputRef 操作。

实测 inventory：

```text
stable_ids=419 navigation=93 buttons=409 interactions=1480 visible_interactions=726 live_samples=302 theme_content=68 inputs=156 selects=24 modal_drawer=13 selection=47 menu=2 dropdown=1 collapse=1 segmented=3 empties=3 results=3 forms=3 form_items=6 date_pickers=2 time_pickers=2 breadcrumbs=2 steps=1 descriptions=1 skeletons=6 popovers=2 notice_holders=2 timelines=1 avatars=260 avatar_groups=1 statistics=2 rates=1
```

## 一条旧资源合同的实际失败和修复

首次八项 CTest 为 7/8，唯一失败是 `rynui.token_gallery_contract`：
`Token Gallery build contract is missing: rounded_effect.vertex.dxil`。
095 已将 shader 直接嵌入最终 exe，因此 Gallery CMake 不再出现旧复制路径。
仅把两条外部 shader 文件名预期替换为 `rynui_embed_target_shaders(rynui_token_gallery)`；
未改生产资源代码、资源输入、字体策略或其余合同。原失败日志、JUnit 和 exit 8 与修复后结果分别保存，未覆盖原记录。
该修改无需重新编译 exe；修复后只重复这组短合同，不重复已通过的六组交互流程。

## 保留性、审查及边界

[父保留性脚本](verify-main-integration.py) 逐项核对当前父 main：390 个旧 IDs、36 个旧样例函数原文及调用顺序、
70 个非目标 overlay 不变；新三个样例函数保持已审 worker 原文。095 的低栈 helper、静态默认 ThemeConfig、
const 引用传递保留。550 个冻结文件逐 blob 相等，另对上述唯一旧资源合同实施精确替换断言。
冻结 manifest SHA-256：`f6acbb4d43cf586657c3dec3619761cd4945f943c0cfa476b2aff9ce49f5ea15`。

clang-format 22.1.3 检查 792 个自有源文件，0 failures；显式 OpenSpec 1.14 doctor healthy、strict 95/95。
独立 Astra xhigh/default 组合源码审查 CLEAN，完整读取 093 范围并补审上述构建合同修正；生产和 frame 源码未漂移。
审查绑定当前 HEAD/MERGE_HEAD 的未提交组合，不冒称审查员重建或执行了程序。
1838 个非 docs/openspec/out tracked 文件的审查 manifest 开始为
`3d59d52c74e661fc82941d100a8876c3410e7eb0a14a55cfe98e8af2db188787`，
结束为 `f825770611c4d409232be8059d1be939a092d272094240117fa9cc942d47ab09`；
仅将合同测试 blob 换回父版本即精确恢复开始值，差异已经补审。原 worker 审查和本次父审查分开记录。

本轮是平台通用 Recording 与构建合同；没有执行完整 frame、journey、native smoke、GPU/人工视觉、Linux、系统 IME 或 AT。
不沿用 079 或前批的完整 frame/native 结果。原构建中既有 MSVC warnings、HarfBuzz CMake 提示及 shell stderr 包装保留，
不声称零 warnings。最终源码、对象、三个 exe 的 hash/时间及 1 MiB PE stack 检查随当前原始日志一并归档。

[原始产物索引](main-integration-artifacts/index.json) 保存 21 项文件的逐字节 SHA-256；
[身份记录](main-integration-artifacts/identities.json) 区分本轮源码、对象和程序。
提交前逐项比较 staged Git blob 与索引，避免换行转换后 hash 失效；原始 UTF-16 PowerShell 日志没有转码或删改。
