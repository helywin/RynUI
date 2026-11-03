# 原生组件页面宿主

2026-11-03，实际 Linux / `linux-gcc` Release / Ninja Multi-Config / BUNDLED。

- `rynui_token_gallery` 构建成功，交互主入口使用 GalleryPagesHost；旧 finite smoke/acceptance 入口仅用于兼容回归。未在用户桌面启动窗口。
- 定向 CPU/模拟文本平台检查共 9 项通过：`gallery_pages_host`、`gallery_sample_page`、`gallery_source_panel`、`gallery_page_controller`、`gallery_document_contract`、`gallery_document_viewport`、`gallery_document_viewport_contract`、`platform_input`、`sdl_event_adapter`。新增项或失败项修正后单独复核，没有执行全量 CTest。
- 覆盖 73 个 catalog 页、五种宽度（1440/960/728/480/360）的正文 lane 几何和 resize 身份、真实组件优先 wheel、目录与正文独立滚动、TOC 不卸页、跨页锚点恢复、别名搜索/Enter、Default/Dark/Compact 主题身份、组合输入不抢键、popup padding wheel、窄目录 Drawer 边界/Escape、键盘焦点显露。
- 长内容在 Gallery 内部视口按 intrinsic 高度测量；命名锚点按活动 generation 登记。离页 hook 将锚点保存到退役页，而不是已选中的新 route。隐藏目录分支退出输入与绘制活动；动态子树沿用原 translation，不改变 Core 公开布局。
- 样例卡以业务标签显示，保留内部稳定 test ID；实际源码面板与公共 API/Token 入口逐卡接入。当前有效组件常用 Token 字段和全局 Token 从 ThemeSnapshot 读取，没有虚构未实现演示或百分比。
- 文档链接与现行交互入口/六章节 identity 检查通过；clang-format 22.1.3 检查 841 个自有源文件，0 failures；OpenSpec doctor healthy，strict 105/105，diff 空白检查通过。

这是无窗口 CPU/模拟文本平台证据与 Linux native 编译结果，不代表实际 GPU 呈现、系统字体/输入服务、系统 IME、真实显示器或 Windows/MSVC 验收。工作量 benchmark、finite 页面验收入口和分平台集成按后续任务记录。原始日志仅在 `out/105-implementation/20261103/`。
