# 新页面视口基础

2026-11-03，Linux / `linux-gcc` Release / Ninja Multi-Config。

构建 `rynui_gallery_page_viewport_tests`，定向 CTest `rynui.gallery_page_viewport`：1/1 通过。覆盖 1440/960/728/480/360 的导航、正文、TOC、header/scrollbar 几何分区；具名 sample 跳转、resize 锚点距离、短页/底部 clamp、generation 退役、拒绝外页节点与旧 generation、同 offset 不重复 translation、追加节点补齐位移以及 window layer 不随正文移动。

这是 shell 接入前的 CPU 基础子阶段；4.1–4.5 的完整界面/滚动/键盘验收仍未完成，checkbox 不变。未打开窗口或执行 GPU/系统输入验证。原始输出仅在 `out/105-implementation/20261103/`。
