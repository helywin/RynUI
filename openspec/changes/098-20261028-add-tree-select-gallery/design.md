# Design

## Context

动机见 proposal。固定父 main `c9cc6c2aab75f03739e0540b47195af1cc897f67` 已完成 097 单值 TreeSelect、Astra R4 和 HEADLESS Debug/Release 78/78。当前 Gallery 有 441 stable IDs、41 个已发布 add_* 样例函数；TreeSelect overlay 仍为 planned，目录未映射实际样例。既有 096 提供联系人／项目树和共同生产锚点、有限视口测试，可沿用其业务交互方式。

## Goals / Non-Goals

**Goals:** 用两个实际保留流程展示已发布 TreeSelect；复用原生 authority、Tree 导航与 Form，不在 Gallery 内复制选择模型。新流程与已有联系人／项目树共存。

**Non-Goals:** 不修改公开 API、Core/Theme/renderer/RC/系统字体/CMake，不扩展多选／搜索／动态拓扑／富节点／异步；不把新 Gallery 作为组件平台专属验收。若发现 Core 问题，先独立复现、修复和验收，再明确更新冻结父，不把 Core 修复夹入本 change。

## Decisions

1. **固定节点和接受值驱动反馈。** 独立目标选择使用受控 String/open/expanded、足以形成有限滚动的叶节点，以及响应式标题与禁用分支。值／关闭接受开关只控制父回写，反馈分别显示请求与接受值，不把候选当成提交结果。备注是独立原生 Input，保存反馈读取实际接受目标与备注；不用重挂载的模拟选择器。
2. **Form 复用现有 String 字段。** 第二个 TreeSelect 绑定必填 FormField<String> 和显式 control(ref)，提供提交／重置按钮。selector 固定为只读显示，不伪造搜索或 preedit；独立备注输入的草稿与原有输入合同分开验证。
3. **恢复与定位沿用生产路径。** 禁用／只读／显隐恢复按钮放在被控制组件之外；TreeSelect 目录 identity 只新增一个共同 anchor 映射。测试点击真实可见目录按钮产生导航请求，再经生产 helper 跳转；不用私有 host setter 或直接平移到新控件绕过入口。外扩焦点预留 margin，popup 使用窗口层实际 clip。
4. **focused 复用 frame 程序。** 新增 --tree-select-only 并接入默认 main；Default/Dark/Compact × 1200/560、900 高有限视口覆盖真实指针／键盘、拒绝重试、清空、门禁恢复、Form 和完整 generation 身份。检查实际 glyph/ordered fill/HitTest/适用焦点四边；停稳后先验证帧请求，再运行 geometry，避免无条件 sync 掩盖唤醒问题。
5. **冻结与验证。** 对父 441 IDs、41 个旧函数体及调用序、72 个非目标 overlay 条目和 Core/API/Theme/golden/RC/CMake/低栈 helper 做精确比较；新 inventory 实跑确定。官方 Windows MSVC windows-msvc-debug、Ninja Multi-Config、最多 2 jobs，Recording 作为平台通用验收；每个最终集合仅执行一次，失败或变更才重跑受影响项。完整 frame/native 留给独立统一验收，不重复历史矩阵。

## Risks / Trade-offs

- [关闭拒绝和焦点退出重叠] → 真实有焦点 outside 点击检查单次回调及下一次重试，不直接调用关闭私有方法。
- [节点标题或禁用更新造成值反馈过期] → 反馈按接受 key 和当前标题派生，与真实 snapshot／文字比较；未知／已禁用接受值按 097 合同保留。
- [窄视口 popup 或焦点装饰被裁断] → 保留有限 document/window clip，检查 glyph/fill/hit 与四边 coverage，不扩大 clip 或降低断言。
- [新增样例影响旧数量／目录] → 追加 IDs／函数，跑精确 inventory、八项 contracts、旧 Tree/AutoComplete 与 Form 代表，旧断言只更新实际新增数量。

## Migration Plan

先验证、提交并推送规划；沿用用户明确的持续开发授权进入 apply，不额外请求已授予的实施权限。按 tasks 完成可独立验证阶段，每次英文 Conventional Commit 后只 push main 并核对远端 SHA。最后安排全新 Astra xhigh/default 审查；不创建 PR 或 archive。回退只撤销本 Gallery 提交，保留已发布 097 Core。
