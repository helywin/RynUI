# 常用组件 Gallery 平台通用集成证据

2026-10-23，Windows x64/MSVC，`windows-msvc` preset、Ninja Multi-Config、Debug，使用固定验证字体与 Recording GPU/draw fixture。此阶段不等同于真实 GPU 或平台输入验收。

## 已完成的交互示例

LiveSamples 末尾新增 Tag/CheckableTag 四例、Badge 五例、一个响应式四边 Drawer、三个 Popconfirm、三种 Alert、一个保留输入内容的 Spin、三种 Progress。页面按钮提供标签恢复、计数更新、提示恢复、加载开关、进度与异常状态联动。Drawer 保留 Input/TextArea/Select、长正文和确认流程；多元素 slots 使用显式 Flex 组合。

`rynui_token_gallery_frame_tests --common-only` 实际运行并返回 0，断言覆盖滚动到入口后的标签选择/关闭/恢复、Badge 计数/归零及圆点/状态实际宽度、四边 Drawer 停靠/编辑/关闭重开/草稿身份/背景滚轮屏蔽、Drawer 内 Popconfirm、受控确认加载/完成、仅确认按钮、Alert 关闭/恢复、Spin 编辑资格屏蔽/恢复、三个 Progress 同步更新及 slot/组件数量不变。

`--inventory-only` 成功输出实际挂载：235 stable IDs、93 navigation controls、275 Buttons、755 interactions、504 visible interactions、262 live samples、67 Theme content runs、116 Input 系列组件、20 Select、13 Modal/Drawer。既有 Modal/Input/Tabs 样例索引保持；相关断言按实际值更新。

catalog generator/contract、reference catalog、document model/contract、viewport/contract 与 benchmark：8/8通过，0.56s，日志 `out/gallery-common-catalog-tests.log`。support overlay 与生成 catalog 将七个已实现组件标为 partial，并明确动画、服务、完整 Web API 等缺口。

## 实际发现与修复

新增 Token 后，MSVC Debug 的旧 add_palette_cells 在单个函数中保留十份大型 ThemeConfig 临时对象。CDB 实际捕获 `c00000fd`，调用栈为 add_palette_cells → reference_cell → ThemeScope → resolve_theme。新增默认主题的 reference_cell 转发重载，避免每个调用点分配大型默认临时对象；保留原调色板、主题与显式覆盖语义。随后实际完整 Gallery 挂载、布局和聚焦交互通过，未扩大线程栈容量。

## 验收边界

本阶段完成聚焦交互和目录合同，完整 token_gallery_frame 留在 task 2.2 单独运行。没有声称这七个新增组件的 Windows 原生 GPU/DPI、Linux、人工系统 IME 或人工视觉通过；Modal 自身的原生证据仍由 change061 独立记录。
