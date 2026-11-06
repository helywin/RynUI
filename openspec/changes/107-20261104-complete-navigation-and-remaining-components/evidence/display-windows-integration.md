# 数据展示首批 Windows 集成（进行中）

平台为 Windows/MSVC，正式 `windows-msvc-headless` Debug/Release、Ninja Multi-Config，串行构建、最多 2 jobs。原始日志、调试器输出和后续图片均只在忽略的 out。

已集成 Card Grid/Meta/Tabs/loading、Empty 动态内容/共同图片/矢量插画，以及 Listy 稳定键控行、网格、分页、滚动和生命周期。List 保持 deprecated，迁移信息属于说明页，不注册假的可运行 List 组件样例。

新增 `rynui.portable.card_empty_completion`、`listy_component` 两个入口在 Debug/Release 均已构建并通过，即每种配置 **2/2**。首轮另运行 Empty/Result 的 component/public_api/theme、Card public_api、Tabs component 和 backend boundaries。Listy 修复了 viewport 父交互缺少绘制条目；行测试按真实 viewport 归属筛选，保留 Pagination 自己的隐藏编辑器，不再误将它视作行编辑器。

旧 Card component/theme 曾出现的 Debug `0xC00000FD` 已通过共享 Theme/Select 减栈修复，结果见 `theme-stack-windows.md`；修复提交为 `5a3d16da`。Card 任意后代圆角遮罩仍缺共同能力，29.1/29.2 未勾选。

Windows 原生 Gallery 的 `ant_design_reference_catalog`、`gallery_sample_page`、`gallery_pages_host` **3/3** 通过。hidden D3D12/DXIL 使用 system fonts，1440/960/728/480 四种宽度及 Default/Dark/Compact 组合，完成 Card、Empty、Listy 三页 **141** 张局部/操作状态捕获（53/48/40），均正常退出、自动扫描无警告。首轮图像暴露 Listy 分页紧贴裁剪行；现按 Theme map 加入 section gap，真实 viewport/pager 几何回归 Debug/Release 各 **1/1**，复拍已确认间隔。

自动操作覆盖图片失败/重试/完成、富说明及操作区移除恢复，Card 真实 Tabs/Grid/loading，以及 Listy 草稿/重排/网格/分页切换/方向/滚动和空态恢复；Listy 的保留编辑及空态清理有实际运行断言。Card 页签定位脚本已改为获取区域内真实 Tabs 实例并成功复拍。全部图片只在 ignored out，结果不计为系统 IME、系统光标或读屏实测。

Empty/Listy 声明的本批原生合同已完成；支持目录保持 partial，独立登记其他平台和 Web 边界。List 保持 deprecated；Util 锁定公开文档只有三个 TypeScript 类型工具，归类 web-only，独立说明页给出已有 C++ typed Props/ref/值类型替代，不创建 UI 空壳。目录生成/自测、源码注册检查和 OpenSpec 校验随提交执行。Card 任意后代圆角遮罩未完成，29.1/29.2 保持开放。所有原生测试 hidden、不抢焦点；启动进程抑制系统崩溃对话框并保留失败码。
