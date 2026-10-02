# Windows 原生验收

2026-10-02，Windows 11 专业工作站版 10.0.26300、MSVC 14.51.36231、Ninja Multi-Config，使用 windows-msvc-debug / windows-msvc-release presets。

- 两种配置全部 native 目标构建通过；最终受影响 CTest 分别 16/16（18.07 秒）和 16/16（5.94 秒），包括 Slider、SDL key adapter、Theme 算法/公开 API/runtime/作用域/allocation/golden、Gallery catalog/frame、平台 lifetime 和 frame renderer。
- 最终 Debug/Release 可执行文件各在系统 display scale 1.25 和显式 render scale 2.0 完成一次真实 SDL/Direct3D 12/DXIL 窗口运行，共 4 次，全部退出 0。每次 22 个归一化输入事件、7 次 change、6 次 completion、14 次实际 frame submissions，content_runs 始终为 1。
- 输入由 SDL_PushEvent 注入并经过实际 PlatformState poll/SDL normalization 路径：up/page keys、独立范围 thumb 焦点、mouse capture 与越界 clamp、端点禁止跨越、reverse/vertical、disabled 阻断与取消全部通过。touch/cancel 等平台通用行为已有 common tests；本记录不声称物理鼠标/键盘或触摸设备人工验收。
- 每次保存 initial / keyboard / dragged / range / dark / compact / resized 七张 GPU 回读，共 28 张 PNG；Debug 系统初始、Release 2.0 Dark 与 resize 代表图人工检查正常。最终补齐 cleanup 后重新运行全部窗口，28 张截图 SHA256 与已检查图完全一致。
- 通过 SDL_SetWindowSize 实际 resize 为 900×720，重取窗口 metrics 并同步布局/scene/upload/readback；没有重新执行 Content。Theme Default/Dark/Compact 与两个 scale 的绘制均出现在截图中。
- windows/runs.json 保存每次命令、exit code、当前最终可执行文件 SHA256、截图尺寸与 SHA256。validate_native.py 验证 4 次实际运行、28 张 PNG 和当前 Debug/Release 二进制一致，结果见 hash-verification.txt。

构建/CTest/app logs、脚本、截图与 metadata 保存在 windows/。平台通用验收不重复；Linux GPU/window/input/DPI/resize 必须在实际 Linux 环境单独执行，当前仍 pending。本记录不包含 GPU 时间、Tooltip、marks/dots 或多端点验收。
