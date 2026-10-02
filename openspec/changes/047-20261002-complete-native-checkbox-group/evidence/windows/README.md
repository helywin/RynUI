# Checkbox Windows 原生验收

2026-10-02，在实际 Windows/Win32 机器使用 VS 2026 MSVC、Ninja Multi-Config 和 `windows-msvc` 完整构建 Debug/Release。受影响 CTest 各 15/15：Debug 28.45s、Release 7.16s。平台通用实现已在 `windows-msvc-headless` 完整通过 Debug/Release 各 48/48，不要求 Linux 重复同一逻辑合同。

`validate_native.py` 运行 Debug/Release × 系统 DPI、1、1.25、1.5、2 render scale，共十个实际 SDL 窗口。系统 display scale 为 1.25；实际 backend 为 direct3d12，shader format 为 DXIL。每次运行规范化 14 个输入事件，主 Checkbox change/click 各两次，独立受控候选与 Group 候选各一次，content 只执行一次，五个手动标签 slot 各执行一次。

每次保存 21 张实际 GPU readback，共 210 张 PNG。普通图片为 1600×1000，resize/inactive 为 1420×900。覆盖 Space 按压/激活、鼠标 hover/按压、有限 wave 的开始/中间/结束、disabled 取消、受控不回写、三类值 Group、动态选项保留重排与 Tab 顺序、删除焦点/捕获、空组恢复、半选富标签 RTL、局部主题、Dark/Compact、resize、失活和销毁。检查了系统缩放初始图及 Release 2× RTL 图，中文、图标、勾选、半选、禁用与局部紫色主题正常。

capture 阶段每张图显式提交两帧，共 42 次；随后三个 native event-poll/动画时钟轮询没有产生 FrameRequest 或额外提交，没有动画 deadline。销毁后 refs、交互、文字和效果清空。共同合同另验证了 wave 重启复用、主题与 reduced motion 取消、回调销毁、token 局部 invalidation 和闲置帧调度。

完整 Gallery 的 Debug 真窗口启动额外暴露了默认栈上的临时主题/样例帧压力。`3ba9cd3` 将大样例函数拆分，并让 themed_button 按引用接收配置；修复后 Gallery Debug/Release 真窗口 smoke 均通过，系统字体实际使用 Segoe UI Variable Text / Microsoft YaHei UI。布局回归再次通过 Debug 25.77s、Release 5.13s；91 stable IDs、108 live samples、57 Theme content runs 保持不变。相关 smoke 与原生构建/CTest 日志随证据保存。

`runs.json` 保存各次启动参数、EXE SHA256、所有图片 SHA256 与尺寸。生成脚本同时核验运行前后 EXE identity、图片集合/尺寸、不同状态 readback 和最终哈希；`--verify-only` 可独立核验已有 EXE 与截图身份。运行需要项目构建产物和带 Pillow 的 Python。Linux 窗口系统/GPU/系统字体/输入验收独立待完成。
