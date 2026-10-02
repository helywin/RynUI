# Windows marks/dots/value hints 原生证据

2026-10-02，代码基线 `1703052`，本证据提交同时保存 native acceptance fixture。真实 Windows SDL 窗口与 D3D12/DXIL，系统 display scale=1.25；下列结果不代替 Linux 原生或用户人工输入验收。

## 构建与 CTest

- `windows-msvc-debug` / `windows-msvc-release` 完整 build 成功，MSVC + Ninja Multi-Config。
- 受影响测试选择：`theme|slider_component|tooltip_component|window_overlay|component_mount|component_scene|gallery`，35 项。
- Debug 首次 33/35 通过；两项 Gallery journey 的旧 inventory 断言未计入新增 6 个 thumb Tooltip wrapper 和 6 个 mark label。更新断言后两项重跑 2/2，27.80 秒；合并结果为 35 项均通过，不声称一次完整运行 35/35。
- Release 同一选择 35/35，11.83 秒。平台通用 headless 完整结果见 retained-labels-and-hints.md，不重复为 Linux 验收。

## 实际窗口矩阵

使用 `evidence/windows/validate_native.py`，Debug/Release 各运行 system、1、1.25、1.5、2 五种 render scale，共 10 次真实窗口运行；全部 exit_code=0。每次 24 次 GPU frame submit、26 个 SDL 归一化输入事件、Content 只执行一次。

每次保留 12 张 GPU readback：initial、keyboard、dismissed、reopened、label、captured、dragged、range、dynamic、dark、compact、resized。验收覆盖 CJK/English marks、点/轨道状态、离散 Range 键盘选择、Escape 保留焦点、新焦点重开、label 点击、capture/clamp、范围不跨越、reverse/vertical、动态 mark identity、Default/Dark/Compact 与 resize=900x960。

120 PNG、10 份诊断日志、可执行文件及截图 SHA256 均记录在 `windows/runs.json` 并校验。已检查 scale=2 keyboard 和 scale=1.25 Dark 图；动态例子的长标签改为紧凑刻度后重跑完整矩阵，避免示例在窄宽度下文字重叠。图像来自 renderer GPU readback，未用生成图替代。Tooltip arrow 的细条实现沿用 040 的近似绘制，精确 mask 将随共用 Icon 能力收尾。

格式检查（clang-format 22.1.3）、doctor、strict 41/41 与 diff check 均通过。Linux 专属任务继续保持未勾选。
