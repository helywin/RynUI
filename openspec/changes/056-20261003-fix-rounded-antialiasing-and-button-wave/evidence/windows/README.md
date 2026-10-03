# Windows 圆角与 Button wave 原生验收

2026-10-03，实际 Windows / MSVC / Ninja Multi-Config / SDL3 / Direct3D 12 / DXIL。系统 display scale=1.25；以下结果不代替 Linux 原生 GPU 验收。

## 结果

- `windows-msvc-debug`、`windows-msvc-release` 各运行系统/1/1.25/1.5/2 render scale，共10次真实窗口。
- 180张 GPU readback PNG，包含浅/深色 coverage、聚焦 TextArea、常规/Primary/Round/Circle/Compact Button 的五个 wave 时刻、重启、reduced、失活、resize。
- 每次对浅/深色各22个 RoundedEffect 区域逐像素比较，共440个区域、6,855,992个像素。RGB 每通道容差2/255，实测最大误差1/255；曲面存在分数 coverage，全部16种角组合与分区接缝匹配参考。
- 曲面填充、1dp细边框、inset、平移/祖先clip、混合角填充与左右单侧圆角边框都进入数值比较。Quad 是同窗口的视觉对照，未改变其 shader，也未用 RoundedEffect 参考冒充 Quad 数值验收。
- 浅色 wave 使用 SDL 键盘 Return，深色 wave 使用 SDL 鼠标 motion/down/up；每个时间点核对独立扩展/淡出和 ease_out_circ：0/100/400/1000/2000ms。400ms 后外扩完成但仍保留 fade，2000ms 后清理。每次34个归一化输入事件、content_runs=1、38次提交。
- reduced 同步取消，失活立即取消 wave，独立 hover color 在200ms内完成；resize 到1420×1000，随后3次idle poll无新增提交或deadline，dispose清空节点/文字/交互。
- Debug/Release Gallery `--smoke` 与既有 `--button-acceptance` 均 exit_code=0；生成/部署/Quad/RoundedEffect shader合同各4/4，headless backend boundary各1/1。

## 图像审核与对应路径

实际查看了 Debug 系统125%的浅/深色 TextArea 与 wave，以及 Release 150%的暗色400ms波纹、Release 200%的完整暗色角组合。观察到圆角分数过渡、单侧圆角与直角保持各自形状、分区无可见接缝，wave色带贴合外缘。

- [聚焦 TextArea 与组件组合](debug-system/textarea-light.png)：复现用户文字、空行、clear action；下方依次为形状变体、左右单侧圆角 Button、上下单侧圆角 Input、SpaceAddon。
- [16种角组合](debug-system/coverage-light.png)：首行为fill、thin-border、inset、空位、Quad；后续mask0至15按行排布。bit顺序为左上/右上/右下/左下；末行还包含translated-clip和左右单侧圆角border。
- [100ms 外扩](debug-system/wave-light-100.png)、[400ms 暗色反馈](release-scale-1.5/wave-dark-400.png)。

## 重放与证据身份

构建 `rynui_rounded_acceptance` 或 `rynui_token_gallery`，使用正式 windows-msvc Debug/Release presets。独立程序接受 `--evidence-dir=<目录>` 与可选 `--acceptance-scale=<值>`；Gallery 使用额外 `--rounded-acceptance` 入口。没有指定scale时使用真实系统display scale。

`validate_native.py` 重放10次矩阵并检查像素、图像尺寸与各wave阶段；`--verify-only` 校验保存的结果。Python需要Pillow。本机使用 Codex primary runtime Python，未修改项目依赖。

`runs.json` 保存每次EXE/log/PNG/CSV/RGB SHA256和逐区域误差。`native-checks.json` 保存Gallery EXE与smoke/Button/shader/boundary日志SHA256。CSV明确LF、RGB明确binary，避免Git行尾转换改变像素参考身份。RGB是按CSV offset排列的packed reference结果；PNG来自GPU readback，不是参考图生成的截图。

平台通用完整CTest与修正说明见 [共同证据](../common/button-wave.md)。Linux task保持未勾选；本次未运行Linux窗口/Vulkan/SPIR-V。
