# Modal 原生行为收口

2026-11-08。原始日志和局部截图只保留于忽略的 `out/resume-20261107/modal-completion/`。

- Windows MSVC `windows-msvc-headless`：Debug、Release 各 **56 个不同合同通过**。Debug 在修正 Gallery 按挂载顺序取 Modal 的旧夹具后定向复跑失败项；Release 最终完整 **56/56**。覆盖 lazy/destroy、有限反转动画、实际指针拖动、指针/键盘缩放、响应宽度、RTL、保留编辑、回调重入、App future、浮层输入域及 Gallery/i18n。
- Windows MSVC `windows-msvc`：Debug、Release 各 **3/3**，实际 hidden D3D12/DXIL RGBA 回读。新增真实 Modal 的透明中间帧、拖动/缩放后位置、退出期间 retained editor 及终帧清理检查；修正 dialog paint opacity 后重新通过。
- Windows Release Gallery：Modal、App 各四组配置，分别 **33、88** 个捕获，退出码均 0、布局警告均 0。Modal 新示例包含有限动画、lazy/destroy、拖动缩放和断点宽度。
- 后续 Linux `linux-gcc-release` 新增 motion/geometry 路径已通过：独立 Mutter Wayland/Vulkan/SPIR-V 回读 **3/3**，Modal 四组配置 **33** 个捕获、零布局告警。设备为 Mesa llvmpipe 软件驱动，完整组合结果见 [Popconfirm/App 收口](popconfirm-completion.md)。

`animated` 默认关闭以保持原有即时呈现语义；启用时关闭立即阻断输入，内容销毁、完成回调和服务 future 等待有效退出完成。显式 RTL、宽度和尺寸更新保留来源优先级及编辑身份。

此结果不代替 Linux 物理 GPU、系统 IME、跨实际显示器或 OS accessibility adapter 验收；共同背景模糊的独立 Linux 结果见 composition-native.md。Linear/Conic paint 和 nonRound cap 仍关闭。
