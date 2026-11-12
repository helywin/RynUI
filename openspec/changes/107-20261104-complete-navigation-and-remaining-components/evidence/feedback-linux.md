# 非 Progress 反馈组件 Linux 验收

2026-11-12，基于 main `521a3e5e6005b5bd3b00e32cb066d38b6b4b1199`；沿用同阶段构建的正式 `linux-gcc` Release Gallery、Ninja Multi-Config、Vulkan/SPIR-V、系统字体及 scale=1。未修改组件生产实现、Theme、共同 paint/cap 或 renderer 能力。

私有 D-Bus/headless Mutter Wayland 下执行两组独立有界序列，均由 runner/native 自动判定通过：

| 路径 | 范围 | 实际结果 |
| --- | --- | --- |
| 正常 surface 提交 | Alert、Drawer、Message、Modal、Notification、Popconfirm、Result、Skeleton、Spin、Watermark，随后 Button | 11 cases、122 checks、11 submitted frames、11 accepted callback markers；host cursor sync=true，未激活 host input-area |
| hidden/offscreen readback | 上述十个反馈页各一次，宽度 1440/960/728/480 和 Default/Dark/Dark+Compact 循环 | 10 cases、125 checks、431 submitted frames、196 个诊断捕获；不执行 surface callback 和 host input-area/cursor 桥接 |

实际操作包含各页已有控制、Drawer 四向有限运动/反转与清理、Message/Notification loading 与终帧 future、Modal/Popconfirm 开关、Result 动态内容、Skeleton active/shape、Spin 延迟/fullscreen 及 Watermark 来源/参数。正常窗口使用现有事件泵和五秒 callback deadline；hidden 的固定时钟/局部 readback 与正常 callback 结果分开计数，不增加人工或 AI 读图门槛。

采用 `tools/run_gallery_page_acceptance.py`，按上述明确页面选择分别不带/带 `--visual`；Linux 始终通过 `tools/run_background_wayland.py`。实际私有修正 Mutter executable、加载 Clutter 及其哈希重新核验，`desktop_fallback=false`，自建 PID 全部退出。一次启动因本地 wrapper 的 TMPDIR 过长触发 Unix socket 路径上限，缩短自有 ignored 临时路径后重新运行；失败轮次未计入通过数，未回退桌面。

Alert、Drawer、Message、Notification、Result、Skeleton、Spin 的原 partial 仅剩平台/Web 或应用服务边界；结合已有 [Windows 平台通用/原生结果](feedback-i18n-windows.md)、[动画收尾](feedback-motion-windows.md) 与本次 Linux 平台覆盖，将其功能状态同步为 implemented，保留应用拥有的网络/状态服务及物理系统 IME、OS accessibility、其他实际 DPI/跨屏和完整 Web API 的明确边界。目录与双语支持说明同步，既有 Modal/Popconfirm/Watermark 功能状态不变。

本次目录/双语字段修改的 catalog、page/document model：正式 HEADLESS Debug **3/3**、Release **3/3**；平台通用 source/i18n 检查另 **4/4**（只在 Debug 选择执行，合为该轮 7/7）。正式 Release Gallery 构建、clang-format 22.1.3、目录生成/自检、OpenSpec doctor/strict 与 diff 检查通过；不重复既有完整 CTest 或原生四配置矩阵。**63.4 仍未关闭，须等待 Progress 完整合入及分类覆盖；53.*、64.*、65.1 均不据此完成。** 本记录不是官方 Web/native 机器差分或逐图黄金比较证明。原始日志、trace、图像、运行身份和二进制仅保存在 ignored `out/`。
