# Tasks

## 1. 平台通用规划

- [x] 1.1 核实已发布 API、旧 Gallery 结构与盘点，创建中文 proposal/spec/design/tasks；OpenSpec 1.14 doctor、all strict 与 diff check 通过后提交推送规划。

## 2. 平台通用交互 Gallery

使用 Windows MSVC `windows-msvc-debug`、Ninja Multi-Config、最多 2 jobs 完成一次平台通用 Recording 检查；不重复历史全套或长 Gallery，不把本组结果描述为 GPU/人工视觉、Linux、系统 IME 或 AT 验收。

- [x] 2.1 在旧样例后追加 Popover 备注、消息通知及 Timeline 记录流程，接通四个真实目录入口，新增 stable IDs、API Gallery 文档、四条 support overlay 和生成目录；新 `--overlays-timeline-only` 验证真实按钮/键盘/输入、同 key 保留更新、关闭/清空、布局/显隐/禁用恢复、三主题双宽 glyph/fill/hit/focus 和 retained 身份，inventory 确认实际新增数量，八项目录 contracts 与 generator check 通过后提交推送。

## 3. 跨流程集成

- [x] 3.1 基线比较确认旧 IDs 顺序、旧样例函数体/调用顺序、非目标支持条目及 Core/API/Theme/goldens/CMake 保持；运行必要旧代表回归、clang-format 22 check、doctor/strict/diff，新一轮 Astra 审查后修复实际问题并记录精确命令/范围/限制，提交推送完成阶段。不重复已通过的定向矩阵，完整 Gallery/native 放在用户指定收尾时统一执行。
