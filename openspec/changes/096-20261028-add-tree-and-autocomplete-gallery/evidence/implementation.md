# 096 实施与主分支验收

2026-10-28。规划已作为 `b1168746fe2929c6049e812907d007adbb39098f` 提交并推送 main，远端 SHA 相等。
实施、R1 修复、新构建与独立 Astra R2 已完成，本提交收口 task 2.1/3.1；只推 main 并核远端 SHA，不自动创建 PR/archive。

## 源码与实际产物

历史起点为 `c1c0bb22a1af2dfc50a5f1b1146cc8e4ef071bbe`；R1 后冻结父更新为已独立发布的 Tree 焦点修复 `a02a2abcd7684b9152f0c5bb40bc5ccce937ad8e`。新增联系人 AutoComplete/表单和项目 Tree 两个样例，附加 22 IDs；既有 419 IDs、39 个样例函数体/调用序、71 个非目标 overlay 和 551 个当前父 Core/API/Theme/golden/renderer/RC/CMake/README blobs 由 [保留性脚本](../verify-baseline.py) 实跑核对。096 提交不混入该 Core 修复。

使用官方 Windows MSVC `windows-msvc-debug`、Ninja Multi-Config、max 2 jobs，真实构建 Gallery、frame tests 和 reference catalog。正式 driver 为 `out/096-main-msvc.ps1`；早期 UI 编译曾复用 093 driver，结果不冒称旧 093 产物。093 的已提交原始日志不变。

实际已通过：

- 最终精确 inventory（exit 0，25.6448191 秒），包含新增组件计数。
- 最终八项目录 CTest contracts 8/8，CTest real 0.26 秒，wrapper 0.3599464 秒；包含 generator check。
- R1 前 focused 六组通过（218.1367508 秒），R1 修复后新构建再次六组全部通过（exit 0，221.4411191 秒）。包括真实目录、选择/清空/受控拒绝、富备注、同值 reset/preedit、树展开/选择/级联与 disabled 隔离、有限滚动、glyph/fill/hit、Input/Button 四边焦点及完整代际；新增反馈逐集合对照 Tree snapshot。
- 当前增量 C++ 实际编译链接通过；最终 clang-format 22.1.3 全库检查 exit 0，保留性与 diff check 通过。
- 最终旧成员/指标/评分代表六组全部通过（exit 0，398.2011506 秒），使用与 R1 后矩阵相同的程序。

```text
stable_ids=441 navigation=93 buttons=422 interactions=1533 visible_interactions=753 live_samples=305 theme_content=68 inputs=160 selects=24 modal_drawer=13 selection=47 menu=2 dropdown=1 collapse=1 segmented=3 empties=3 results=3 forms=4 form_items=7 date_pickers=2 time_pickers=2 breadcrumbs=2 steps=1 descriptions=1 skeletons=6 popovers=2 notice_holders=2 timelines=1 avatars=260 avatar_groups=1 statistics=2 rates=1 auto_completes=2 trees=1
```

## 已观察到的失败及修正

1. 测试初次使用不存在的 `InteractionRegistry::eligible()`，改为查询实际 record 资格与 branch_active。一次诊断误插入旧 Navigation 测试作用域导致编译错误，已撤回并移入新场景；旧函数没有遗留该修改。
2. 首次 focused 的实际 glyph 为 x=279，而新标题分配区从 x=280 开始；给新标题/反馈留 1 dp 左 margin，保留原墨迹断言。
3. 富建议起初纵向组合标题、Input 和 Button，而 AutoComplete 行高固定，Input 实际高度为零；改为横向 Small Input/Button，并用现有行 padding 容纳装饰，不改 Core 或放宽点击断言。
4. 清空后再次输入会重开建议，覆盖下方命令；真实操作先 Escape 再点击命令，保留 topmost hit 断言。
5. 空 `TextCommitted` 不会删除选中文字，旧查询仍过滤掉富建议；测试改用真实 Ctrl+A、Backspace，随后实际富 Input 命中、焦点、保存和门禁路径通过。不是 Core 焦点故障。表单提交前同样先 Escape 关闭覆盖按钮的建议。

6. `Tree` 前缀还匹配 `TreeSelect`；真实目录定位改为精确 caption，不改变生产导航。Tree 选择允许取消，测试检查 pointer/Enter 的实际切换，再断言接受值；fill 检查对应真实 checkbox 分配区域。
7. 显式“恢复项目备注”收起项目分支，使富编辑器在树内可见后恢复焦点；不清除草稿或 checked 状态。测试实际展开资源分支并用 End 滚动到富标题。

原日志位于 `out/096-focused-attempt1.log` 至 `attempt10.log`、`out/096-main-members.log`、`out/096-first-test-build.log`、`out/096-diagnostic-scope-build.log`、inventory/contracts/build 日志和 result JSON。失败诊断、R1 前通过与 R1 修复后产物分开保留，不覆盖后冒称同一最终成功。

## Astra R1 与真实修复

R1 为 source-only，发现两项 P2；它没有运行父二进制，也没有给出 CLEAN。

1. 归档 disabled 改变后，反馈缓存未重算。新增真实操作在修复前复现（exit 1，61.763517 秒）：
   `勾选=guide · 半选=project,docs`，但实际 `docs.checked=1 / docs.half=0`。
   现在只将固定项目叶子 key 作为受控 authority，分组全选/半选由叶子及当前 disabled 派生；不缓存 onCheck 时的半选。
   源代码/文档/16 个资源属于该固定业务树，保留 notes 的独立叶子选择；没有增加另一套通用 Tree 模型或改变 Core 勾选算法。
   新回归逐集合比较当前 Tree 的 checked/half_checked，包含解锁→只选指南→再禁用归档的原复现。
2. 首末行 outline 被树根裁剪。该既有 Core 缺口在 094 单独复现、修复并发布，见
   [真实 before/after、26/26 双配置和硬裁剪证据](../../094-20261026-add-native-tree/evidence/boundary-focus.md)。
   保留根硬 clip，以 Theme token 预留内部焦点空间，不放宽到窗口。Gallery 引用该准确父，无重复生产修复。

R1 核查确认 Left/Right 早返回不构成 modality 缺陷：当前 FocusManager 在 command filter 之前已切换 keyboard modality，因此未修改该逻辑。
R1 后矩阵运行时 Core 源码已包含最终修复；过程中只将同样源码单独提交发布，未改变生产或 frame 文件字节。
R1 前旧成员/指标/评分代表已通过 399.583222 秒；最终新产物又实际通过 398.2011506 秒，两组结果分开保留。

## 独立 R2 与最终产物

Astra xhigh/default R2 为 source-only、CLEAN，完整审阅 096 范围及 `b1168746 → a02a2abc` 的独立 Core 小修。
核对全部 25 个固定节点、四组汇总、disabled 与独立 notes；首末行留白、scroll/reveal、根及 document/hit 硬裁剪和原关键回归保留。
审查员只读运行保留性与 diff check，未执行构建或程序，不能把父新构建记录写成其独立执行。
关键源码在审查初末不变：

| 文件 | SHA-256 |
| --- | --- |
| Gallery definition | `e3ea45c556d7ec54015e9df96557d536f31fb8473d6beb8e17469e53ae9f8b7a` |
| Gallery frame tests | `e6a22365861c2da65aa3c0dfa0cb1ab1497ef7fa2d3b158b80a02ae4cd566adb` |
| Tree Core | `358225734317f24a50bc4aacc04ea01ee58c3e62b3c74c693783d148f7382461` |

[逐字节原始索引](windows/index.json) 与 [源/OBJ/LIB/EXE 身份](windows/identities.json) 记录本轮程序、1 MiB stack 和日志。
最初 configure 日志属于该正式输出目录的前期配置，后续实际增量 build 与 Ninja 依赖重建分开保留，不冒称从零重建。
提交前对照 staged Git blob 检查全部原始 hash；UTF-16 日志不转码。OpenSpec 使用显式 1.14，doctor healthy、strict 96/96。
没有执行完整 frame/journey/native smoke，不扩大为完整 main、GPU/人工视觉、Linux、系统 IME 或 AT 验收。
