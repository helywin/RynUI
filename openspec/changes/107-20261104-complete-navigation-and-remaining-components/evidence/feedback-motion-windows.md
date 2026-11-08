# 反馈组件与 Drawer/Notice 动画：Windows 收尾结果

实际环境为 Windows/MSVC，Ninja Multi-Config、默认栈，最多两个编译进程。以下为本阶段新增实跑结果；历史 E1 的 182 项回归及 1,520 张图只作为继承证据，不累加为本次通过数。

- `windows-msvc-headless` Debug、Release：按实际测试注册名运行 Drawer motion、Notice loading、App/Panel service、统一浮层、旧组件及输入/清理定向合同，分别 **67/67**；Popover、Statistic、Tooltip、Alert、Result、Skeleton、Spin 的另一组分别 **32/32**。两组无交集，各配置合计 **99/99**。
- 构建对应 target 后使用 CTest 的 `drawer_motion_*`、`notice_loading_*`、`app_panel_*`、`window_overlay_*`、`popup_statistic_*`、`popover_*` 等定向入口；未重复全仓矩阵。覆盖同 key 编辑保留、受控请求、反转、嵌套、失活停帧、销毁/回调重入及服务 future。
- `windows-msvc-release` Gallery，hidden SDL/D3D12、DXIL、GPU readback：Drawer 的 Default/1440、Dark/960、Dark+Compact/728、Default/480 四配置通过 **96 个四向开合像素/几何时点**及 **4 组嵌套、反转、缩放、清理序列**；112 张局部/状态捕获，0 告警。
- 同一原生路径的 Message、Notification Loading 序列各通过 **16 项**状态、保留编辑器、旋转/停帧和 future 检查，分别产生 26、34 张局部/状态图，0 告警。每端另裁取四张实际图标像素图；两旋转帧的 RGB 字节平均差分别约 **9.42、10.83**，通过图标区域变化断言，不以整张通知变化替代旋转验证。

本阶段修复了两处实际生产问题：较低 sibling Drawer 在退出中重开保留绘制顺序，避免父 push 瞬跳；destroyOnHidden 的旧 Input 清理回调重开时，先完成旧代际释放，再挂载新正文，避免复用仍绑定的 InputRef。关闭立即停用输入；正文销毁、完成回调与 Drawer 服务 future 跟随退出终点的 CPU scene 同步。

支持说明、双语目录和 Gallery 生成结果同步。格式、目录生成/自检、i18n inventory、OpenSpec doctor/strict 与 diff 检查随本提交执行。Popover 等七项已有原生结果继续引用 [E1 记录](feedback-i18n-windows.md)，本次不声称为它们新跑完整截图矩阵。

未新增 Linux/Wayland/Vulkan、物理跨显示器、系统 IME 或 OS accessibility adapter 的实测覆盖。Modal 完整呈现、Card 合成和 Progress 数值/GPU 能力仍独立跟踪。原始日志、截图和运行索引仅保留在忽略的 `out/`。
