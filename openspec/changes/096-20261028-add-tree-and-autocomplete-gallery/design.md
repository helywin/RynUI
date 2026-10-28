# Design

## Context

动机见 proposal。父 main `c1c0bb22a1af2dfc50a5f1b1146cc8e4ef071bbe` 已提供两个独立原生 host、公开 typed API、完整 Theme 链和已审焦点/失败退役合同。当前 Gallery 有 419 stable IDs、39 个追加样例函数；目录 anchor 通过标题标识定位。AutoComplete/Tree 两条 overlay 仍为 planned。

## Goals / Non-Goals

**Goals:** 接通联系人建议与固定项目树，使用真实原生事件体现业务反馈、受控 authority、输入门禁与保留内容。新增样例追加到成员/指标/评分之后。

**Non-Goals:** 不改变组件 API/host/Theme/资源/构建，不新写搜索服务、动态树模型或 renderer 路径；不把图形快照代替真实交互，不扩展网络、异步、虚拟化、拖放、RTL 或平台验收。

## Decisions

1. **联系人使用实际 AutoComplete。** 一个独立联系人输入展示 substring 过滤、disabled 项、候选更新、backfill、清空和可拒绝的受控值；一个 Form String 字段使用现有 `.control(ref)` 及相同值 reset。富建议 label 只构造一次，以稳定 key/共享 label identity 保留备注 Input 和保存按钮；普通选项更新不能重建其 editor。替代方案是重新挂载建议列表，会破坏本次要展示的草稿合同，因此不用。
2. **项目树保持固定拓扑。** 一个有限高度、可级联勾选的 Tree 使用受控 expanded/selected/checked keys；展示根、文件夹、禁用分支和足够产生滚动的叶子。一个富标题含原生备注 Input/保存 Button。回调直接接受候选并更新显示反馈；显隐与全树 disabled 的恢复按钮放在树外。只修改 props，不生成新的 TreeItem 拓扑。
3. **沿用生产导航与布局。** 两个目录 identity 映射到新标题前缀，测试通过真实可见目录按钮产生导航请求，再调用同一 anchor helper。正文按 document_width 绑定，控件给外扩焦点留明确 margin；popup 使用窗口层 clip。不得为了定向测试直接改生产控件状态或跳过目录入口。
4. **一组 focused 验收。** 在既有 frame 程序追加 `--tree-autocomplete-only` 并纳入默认完整入口，复用现有 Fixture/真实 fonts、输入和场景服务。每个新样例在 Default/Dark/Compact × 1200/560、900 高视口下运行实际输入/选择/键盘和门禁。比较实际 glyph-instance/shader clip、ordered fill、HitTest 与四边 focus coverage；ComponentId、editor、node 和 interaction 比较完整 generation。仅对实际拥有焦点语义的控件检查 focus。
5. **父基线冻结与增量发布。** 新样例前的 419 IDs、39 个 `add_*` 函数体与调用序保持原文；仅两条支持条目修改为如实 partial，其他 71 条保持。Core/API/Theme/goldens/renderer/RC/CMake 及 095 的低栈 helper 全部逐 blob/原文比较。新 inventory 由程序实测，不用静态加法代替。使用官方 Windows MSVC `windows-msvc-debug`、Ninja Multi-Config、max 2 jobs；Recording 只算平台通用验收。

## Risks / Trade-offs

- [富建议中的 Input 与候选选择竞争] → 从主 Input、富 Input 和外部控件真实操作，检查 Tab/Escape 与草稿/accepted value；不调用私有 host 选择方法。
- [隐藏树节点留下 preedit 或旧 popup] → 用真实 composition 与富控件焦点验证关闭/恢复，普通展开/勾选更新只保留而不重挂内容。
- [添加下方样例改变旧跳转位置] → 保留原 anchor 逻辑和样例函数，跑受影响旧代表及八项目录合同；不弱化旧断言。
- [矩阵耗时和证据重复] → 一次最终新构建 focused、一次 inventory、短合同和必要旧代表；只在实际失败/变更后重跑受影响部分。完整 frame/native 在统一收尾时执行，不要求 Windows/Linux 重复相同逻辑。

## Migration Plan

先提交并推送规划，随后按现有连续开发授权进入 apply。两个 Gallery 和其共同 focused 作为一个可独立验证的实施阶段，最后完成新 Astra xhigh/default 审查、保留性/文档和主分支发布。每阶段英文 Conventional Commit 后只推 main、核远端 SHA；不创建 PR 或 archive。回退按该 Gallery 提交，不回退已发布 Core。
