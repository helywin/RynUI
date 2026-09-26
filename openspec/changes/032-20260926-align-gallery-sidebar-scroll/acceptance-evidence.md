# 验收记录

规划阶段：用户确认参照 Ant Design 6 官方组件页，并要求可见滚动条。官方组件总览和 Layout 文档已核对；正文目录内容仍以仓库锁定的 Ant Design 6.6.5 数据为准。本 change strict validate 通过；全仓 strict 为 26/32，既有 change 013、015、016、017、018、021 失败。当前 OpenSpec CLI 不支持 `doctor --json`，报 `unknown command 'doctor'`；`git diff --check` 通过。此记录只代表规划与静态校验，不代表代码或真实窗口已完成。

滚动区基础：`GalleryScrollRange` 统一校验 extent、夹紧 offset，`GalleryScrollTranslation` 只平移指定 generation 有效的子树；文档 viewport 改用这两个对象但保留 section/category 锚点、resize 和诊断。平台通用测试在 Windows MSVC Debug 运行 `rynui.gallery_document_viewport` 与 `rynui.gallery_document_viewport_contract` 2/2 通过，新增双区域 offset 与 Node 子树隔离、同值不脏、失效根拒绝断言；源码合同已跟随迁移到独立 helper。本阶段尚未把独立滚动接入 Gallery 窗口，也未报告滚动条可用。

滚动条与装饰：`ReferenceSurface` 增加顶栏、轨道和滑块的 Gallery 内部角色，颜色取自 Theme，装饰角色不生成状态文案或交互；`GalleryScrollbarController` 支持指针拖动与轨道翻页，几何按可视比例及最小滑块长度计算。Windows MSVC Debug 定向构建成功，`rynui.reference_surface`、`rynui.gallery_document_viewport`、`rynui.gallery_document_viewport_contract` 3/3 通过；测试覆盖短内容隐藏滑块、比例/最小尺寸、拖动映射、轨道点击及装饰层语义。本阶段只完成组件，尚未接入 Gallery 布局和真实窗口。

布局阶段：导航由横向换行按钮改为纵向文档入口、七个类别及锁定目录的 73 个静态组件条目；站点栏作为最后绘制且优先布局的 56dp Theme 装饰面，位于两列之上。Windows MSVC Debug `rynui.token_gallery_frame` 1/1 通过，覆盖顶栏高度与纵向位置、导航 97 个直接子节点、宽窄列布局及组件 identity 保持。本阶段尚未把滚动和顶栏拦截接入运行时。
