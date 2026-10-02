# Space 对齐与方向阶段验收

2026-10-03，Windows，MSVC / Ninja Multi-Config；平台通用逻辑使用 `windows-msvc-headless`。

- Debug / Release：Space component/features/public API/header isolation 与 Flex features，均 5/5；CTest 0.46 / 0.40 秒。
- 覆盖 horizontal Auto Center、vertical Auto Stretch、显式 cross 尺寸、typed orientation/vertical 最后配置订阅、非法更新恢复、真实 Text/Button 基线、嵌套 baseline RTL 不重复测量、销毁订阅与资源。
- 旧 Gallery Space 调用显式保留 Start；`windows-msvc` Debug Gallery frame CTest 1/1，23.22 秒。该结果是集成合同，真实窗口/GPU 验收留在 Windows 独立任务。
- 新默认值和迁移方式记录在 `docs/flex-space.md`。

日志为工作区 `out/space-layout-{debug,release,native-debug}.log`；本阶段不将 separator 或 Compact 描述为已实现。
