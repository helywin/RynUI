# 验收记录

规划阶段：用户确认参照 Ant Design 6 官方组件页，并要求可见滚动条。官方组件总览和 Layout 文档已核对；正文目录内容仍以仓库锁定的 Ant Design 6.6.5 数据为准。本 change strict validate 通过；全仓 strict 为 26/32，既有 change 013、015、016、017、018、021 失败。当前 OpenSpec CLI 不支持 `doctor --json`，报 `unknown command 'doctor'`；`git diff --check` 通过。此记录只代表规划与静态校验，不代表代码或真实窗口已完成。

滚动区基础：`GalleryScrollRange` 统一校验 extent、夹紧 offset，`GalleryScrollTranslation` 只平移指定 generation 有效的子树；文档 viewport 改用这两个对象但保留 section/category 锚点、resize 和诊断。平台通用测试在 Windows MSVC Debug 运行 `rynui.gallery_document_viewport` 与 `rynui.gallery_document_viewport_contract` 2/2 通过，新增双区域 offset 与 Node 子树隔离、同值不脏、失效根拒绝断言；源码合同已跟随迁移到独立 helper。本阶段尚未把独立滚动接入 Gallery 窗口，也未报告滚动条可用。
