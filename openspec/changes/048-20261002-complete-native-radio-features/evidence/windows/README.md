# Radio Windows 原生验收

2026-10-03，实际 Windows/Win32 机器，VS 2026 MSVC、Ninja Multi-Config、`windows-msvc` Debug/Release 构建。受影响平台 CTest 各 15/15（Debug 26.88s、Release 7.99s）；此前平台通用 `windows-msvc-headless` 完整 CTest 各 54/54（89.31s/15.56s）。不要求 Linux 重复同一共同逻辑合同。

`validate_native.py` 完成 Debug/Release × 系统 DPI、1、1.25、1.5、2 render scale 共十个真实 SDL 窗口。系统 display scale 1.25，实际 renderer direct3d12、shader DXIL；每次规范化 18 个输入事件。主 Radio change/click 各两次，独立受控候选和 Group 候选各一次；content 仅一次、四个手动 label slots 各一次。

每次 27 张真实 GPU readback，共 270 PNG。常规图 1600×1000，resize/inactive 图 1420×900。覆盖 Space 按压/选中、鼠标 hover/按压、圆形及按钮有限 wave、disabled 取消、受控不回写、三类值 Group、方向键、动态保留重排、删除焦点/capture、空组恢复、solid large block、RTL 连接角、vertical small、切换 circle、独立紫色主题、Dark/Compact、resize、失活和销毁。已查看系统缩放初始图及 Release 2× RTL 图，中文、字形、外角与连接边正常。

每次截图显式提交两帧，共 54 次；之后三个 native event-poll/动画时钟轮询没有新 FrameRequest、动画 deadline 或额外提交。销毁清空 refs、文字、交互与全部 rounded effects。`runs.json` 记录启动参数、EXE SHA256、每张 PNG 的 SHA256 与尺寸；脚本核验运行前后身份及状态差异，`--verify-only` 可检查当前 EXE 与保存图片。

原生 Gallery 回归发现窄约束下 RadioButton token 半径超过实际半边长，87de8de 对该半径收敛并新增 0dp/4dp 回归；HEADLESS Debug/Release Radio features 重跑通过（0.70s/0.47s），原生 Gallery frame Debug 22.25s、Release 随受影响 CTest 通过。同时计入新单一 Tab 组锚点，修正旧 Gallery 交互数。完整 Gallery Debug/Release 真窗口 `--smoke` 通过，实际系统字体 Segoe UI Variable Text / Microsoft YaHei UI，98 stable IDs、114 live samples、58 Theme content runs 保持稳定。

408 自有源通过 clang-format 22.1.3；OpenSpec doctor/strict 和 Git diff 检查通过。Linux 的 window system/GPU/shader/system font/input/DPI 验收需实际 Linux 机器，独立任务保持未勾选。
