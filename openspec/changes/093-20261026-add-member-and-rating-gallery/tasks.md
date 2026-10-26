# Tasks

## 1. 平台通用规划和组件依赖

- [x] 1.1 核实现有 Gallery、三组件公开 API/实际审查状态与目录条目，完成中文 proposal/spec/design/tasks；显式 OpenSpec 1.14 doctor、all strict 和 diff check 通过后提交推送规划，不描述为功能完成。
- [x] 1.2 确认 088/089/090 分别经独立 Astra 审查、main 实际新构建定向 Debug/Release 与推送核 SHA，通过后记录准确冻结基线；三个公开 headers/host 可由该 main 构建，才进入 Gallery 源码实施。

## 2. 平台通用交互流程

使用 Windows MSVC `windows-msvc-debug`、Ninja Multi-Config、最多 2 jobs 完成一次平台通用 Recording；不把它计为 GPU/人工视觉、Linux、系统 IME 或 AT 验收。

- [ ] 2.1 追加成员头像、统计与评分样例和三个真实目录入口，使用实际控件验证业务反馈、overflow、精度/格式化、loading、半星/清空、编辑门禁与富提示草稿恢复；新 `--members-metrics-only` 接入默认完整入口并通过三主题双宽实际 glyph/fill/hit/适用的四边 focus 与 retained 身份检查，实测 inventory 后更新三个 overlay、生成目录和 API Gallery 文档，八项 contracts/generator check 通过后提交推送。

## 3. 跨流程集成

- [ ] 3.1 比较保留 390 旧 IDs、36 旧函数体/调用顺序、70 非目标支持条目及冻结 Core/API/Theme/goldens/CMake，运行受影响的旧代表、format 22、doctor/strict/diff；新独立 Astra 审查并修复实际问题，按精确命令、当前产物和覆盖边界记录证据，提交推送收口。不重复已经通过的定向矩阵，完整 Gallery/native 在用户指定收尾时统一执行。

2026-10-26 隔离 worktree 已完成技术实施、一次最终 fresh 六组 focused、精确 inventory、八项 contracts 和三个受影响旧代表；见 [本地证据](evidence/02-gallery-implementation.md)。2.1/3.1 的父 fresh Astra/main 发布尚未执行，两项保持 unchecked；本子任务只保存本地提交，不访问远端。
