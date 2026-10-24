# Design

## Context

main `7ff4deb` 已包含 Form 首版与主分支继承修复；其 worker Astra R2 通过，父差异 R3 正复核。077 和 078 独立分支尚未合入，不把其规划或已写代码当作 Gallery 已可用。076 已验证真实目录请求→共用锚点→实际样例，并保留全部参考卡片。

现有 FormField 支持 String、optional number、bool、SelectSelection，公开 get()/set()/value()/onChange() 可做显式适配；日期/时间不在 typed FormValue 中。已有 GalleryState 保存 Signal、真实组件与 typed slots，Recording test 有有限视口、实际输入/glyph 和稳定身份检查。

## Goals / Non-Goals

**Goals:** 增量展示真实录入闭环，复用生产导航及现有控件；按依赖独立提交，每阶段验证自身，最后一次完整 Gallery 验证全组。

**Non-Goals:** 不新增表单 schema/通用适配器框架、网络持久化或服务；不修改组件首版 API 来方便样例，不实现 DatePicker/TimePicker 已明确排除的能力，不改变 renderer。

## Decisions

1. 在现有 Empty/Result 之后追加业务录入区。先追加四种现有字段及 submit/reset/disabled/readOnly 的 Form 样例；日期、时间组件分别完成验收并合入后追加对应样例及组合字段，不使用占位控件。标签与反馈使用保留 Text，业务状态存放 GalleryState。
2. 日期/时间的组合字段使用 FormField<String>，通过公开 get() 的 Binding 解析成 picker value；在 FormItem content 内捕获该注册代际的 onChange，再格式化回写。空值映射空字符串，控件 ref 连接实际失败焦点/reset。只读映射遵守 picker 首版合同，不把 inputReadOnly 误当整个选择器不可编辑。必须实际测试 reset/拒绝值/草稿，不能只证明类型能编译。
3. 扩展 076 共用锚点中的明确 identity→稳定样例标题映射；Form/DatePicker/TimePicker 目录到实际样例，其余组件/分类保持原行为。复用生产锚点和跳转，由真实可见目录按钮触发测试，避免手工定位绕过入口。
4. 各阶段在 windows-msvc / MSVC / Ninja Multi-Config 的 Recording 聚焦入口验证真实输入、错误/结果、glyph/clip 与保留身份，测量实际 inventory 后更新断言，不猜数量。三个 Theme、1200/560 宽视口保持有限 900 高；完整长文档 fixture 仅用于旧完整 inventory 流程，不替代样例可达性。
5. 各阶段补对应 API docs/支持范围、generator/contracts 与格式/doctor/strict/diff；最终全部样例齐备后运行完整 Gallery frame+八项合同及 Windows 原生 smoke。没有行为变更时不重复完整矩阵，不把 smoke 记为人工逐控件/GPU/系统 IME/Linux 验收。

## Risks / Trade-offs

- [异步分支 API 或交付顺序变化] → 明确 075/077/078 依赖，只使用已合入真实 API，阶段 checkbox 分别验收。
- [Form 日期时间适配掩盖受控值/重置问题] → 用真实录入、清空、提交和 reset 回归，Form 数据仍为文档声明的 String，不新增隐式转换机制。
- [新样例扩大长文档及验证时间] → 各阶段独立聚焦，完整 frame 最后统一一次；不删除既有断言，不弱化 clip/导航合同。
