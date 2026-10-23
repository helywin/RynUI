# 保留对话框阶段验证

2026-10-23，实际Windows/MSVC x64，`windows-msvc-headless`，Ninja Multi-Config。

## 交付范围

可挂载的Modal一次预建并保留内容、标题、自定义页脚、默认Button及关闭按钮。正常流owner不占Box/Flex/Space或顶层正文的尺寸和gap。实际显示分支、遮罩、受控请求/加载、取消原因、生命周期通知、打开顺序、嵌套边界、有效触发恢复及窗口失活后的延迟恢复均已接入。

对话框按有限窗口夹紧，标题和页脚固定，正文滚动并显露焦点与超高TextArea插入点。窗口层不继承文档clip；命中处理Node内容裁剪和独立窗口层边界，resize更新全窗口命中范围。模态裁剪上下文中的quad复用已有四角fill effects，保留源索引、阴影顺序和fill身份，不修改CPU scene v3/GPU ABI v2或shader。背景Card不因逻辑后代Modal接收hover，Tabs不会透过覆盖的Select菜单处理滚轮。

## 验证

- 真实Input/TextArea/Select/Tooltip/Tabs/Card/Button合同：初始隐藏autofocus、父级拒绝、各取消入口与加载、IME/子层/Modal Escape优先、编辑器及菜单/标签栏滚轮、草稿与身份保留、Tab/无入口、父子关闭/卸载、失效/禁用/隐藏触发、窗口失活关闭与恢复、回调重开/卸载、捕获取消和全部资源释放。
- 1/1.25/1.5/2字体缩放：正文滚动不重新shape或重建已实现字形，不重新测量正文；滚动保持物理像素相位。
- 视口缩放到1×1后恢复；超高编辑器首尾caret显露、手工内部滚轮不被撤销；遮罩覆盖文档clip以外及resize后的新窗口区域；正文控件不能在标题区域命中，默认页脚focus保留对话框padding。
- 极值层级：Modal使用INT_MAX、所属Select使用1时菜单仍在所属对话框之上且可越界命中；全局层级base与打开顺序各自有效。
- 局部纯颜色更新不测量或重跑正文；无输入的稳定Modal不申请帧或deadline；关闭/销毁后无边界、会话、surface或effect泄漏。

完整Debug/Release构建成功，完整CTest各 **115/115**，耗时 **212.89s / 11.40s**。`input_scene_allocation`使用256个Input，selection与composition-selection各20000次更新，两种配置均为`allocations=0`、`dispatch_allocations=0`、`sync_allocations=0`。修复后受影响裁剪/输入/窗口层/既有控件专项 **20/20**（Debug，4.57s）。

日志与JUnit：`out/modal-host-clip-regression.log`、`out/modal-host-debug-full.log`、`out/modal-host-debug-ctest.xml`、`out/modal-host-release-build.log`、`out/modal-host-release-full.log`、`out/modal-host-release-ctest.xml`。clang-format22.1.3检查496文件，0失败；OpenSpec doctor健康，strict validation 61/61，`git diff --check`通过。

## 边界

这是平台通用逻辑、保留场景和Recording/无窗口验证。Gallery集成、Windows D3D12/DXIL真实窗口、系统字体/DPI及Linux native验收尚未完成；自动composition不计作人工系统IME。Input自身的caret blink和Button加载动画仍遵守既有合同，无Modal轮询定时器。
