# 布局与导航页面样例

2026-11-03，实际 Linux / `linux-gcc` Release / Ninja Multi-Config / BUNDLED。

- 提取 Typography/Divider 原目录卡实例与混合样例；登记 Typography、Divider、Flex、Space、Icon、Breadcrumb、Steps、Menu、Dropdown、Pagination、Tabs。累计 14 个 canonical 组件页、29 个实际样例组，来源函数与公开头文件路径有效且 ID 唯一。目录状态数据保持不变。
- 构建 `rynui_gallery_sample_page_tests`、`rynui_token_gallery_frame_tests`：通过。定向 CTest `rynui.gallery_sample_page`：1/1 通过；旧 frame `--navigation-only`、`--data-only`：2/2 通过。
- 新夹具逐页验证工厂隔离和非活动 surface 退役；验证 Typography 原文/省略/Link、Divider 线型/RTL、Flex 拉伸、Space Compact/AddOn、Icon 旋转/source/spin、Breadcrumb 显隐/草稿身份、Steps 当前项/status/reset、Menu 选择/禁用、Dropdown 打开/Escape、Pagination 受控页码、Tabs 面板切换与编辑保留。
- 补齐页面夹具的既有 SelectionComponentHost 依赖，保留 Space 中真实 Radio 组合；未用删例规避宿主要求。Pagination 与 Table 联动保留独立目标身份，避免重复历史 ID 归属。
- 此阶段未替换原生文档 shell，也未完成其余组件、全部历史 frame/global inventory 假设及代码展开 UI。既有导航回归仍是兼容夹具，新 shell 的导航/几何验收在 4.5。

以上为平台通用无窗口组件/CPU scene 回归，不代表 SDL、真实 GPU、系统 IME 或 Windows/MSVC 通过。原始输出只在 `out/105-implementation/20261103/`。
