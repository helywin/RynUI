# Windows RadioButton 与 Switch 补充验收

2026-10-03，实际 Windows / MSVC / Ninja Multi-Config / SDL3 / Direct3D 12 / DXIL。系统 display scale=1.25。此项对应056任务5.1；Linux5.2仍独立待完成。

## 结果

- `windows-msvc-debug`、`windows-msvc-release` 构建更新后的 `rynui_token_gallery`，各运行系统/1/1.25/1.5/2 scale，共10组窗口运行；每组独立运行 `--switch-acceptance` 和 `--radio-acceptance`。
- Switch 每组Middle/Small、LTR/RTL、双向切换共8个过渡，各捕获base/按下0/50/200ms/释放0/50/200ms，共560个实际GPU帧。原始逻辑几何与单调时间记录在CSV；释放0ms与按住200ms的bounds完全一致，之后宽度与位置同时变化，200ms恢复另一侧圆形。
- 50ms阶段数值检查采用CSS ease-in-out的25%时间进度0.12916193；另外逐帧扫描实际GPU白色手柄，要求左右边缘与几何相差不超过2物理像素。所有560帧通过。
- RadioButton 每组浅/深、LTR/RTL、横/竖8种Solid组合，共80个样本。独立SDF参考与实际readback检查913,384个曲面/边界内部像素，包含共享边的所有完全覆盖内部像素；RGB通道容差2/255，最大误差1/255。外部共享边属于异色邻项，不作为窗口底色参考比较，其内部仍要求纯选中颜色；CPU scene测试还覆盖连续亚像素位置。
- 每组保存56张Switch局部readback、8张Radio局部readback、2张Radio全窗口图，共660张PNG。局部截图按记录的几何统一裁剪，未重绘像素；原始BMP留在ignored `out/056-feedback-native`。
- SDL归一化键盘与指针、受控值、取消/resize/idle/dispose沿用原生fixtures并实际通过。Gallery Debug/Release smoke、Button acceptance通过；Gallery frame/platform journey/catalog contract各3/3通过，见 [Gallery日志](gallery-ctest.log)。smoke/Button与EXE身份更新至 [原Windows native-checks](../windows/native-checks.json)。

## 图像审核

实际查看了 [125%手柄阶段表](switch-timeline.png)、[浅色左右连接按钮](debug-system/solid-light-ltr-horizontal.png)、[150%竖直连接按钮](debug-1.5/solid-light-ltr-vertical.png)、[Release 200%暗色RTL](release-2/solid-dark-rtl-horizontal.png)。手柄按住时向内伸长，释放0ms保持该形状，50ms边移动边收缩；蓝色填充内部及选中共享边没有浅线，单侧圆角与直角保持正确。

阶段表只排列原始GPU局部截图；文字slots仍遵循既有切换显示合同，本次修正手柄几何过渡，不将slots的新滑动动画列为已实现。

## 重放与证据身份

使用正式native presets构建 `rynui_token_gallery` 后，运行 `validate_native.py` 重放矩阵；Python需要Pillow。`--verify-only` 校验10组矩阵、EXE、CSV/log/PNG SHA256、时间/几何和GPU像素。`runs.json`记录全部身份、缩放与统计；CSV/log/json固定LF，PNG为binary。本机使用Codex primary runtime Python，未增加项目依赖。

原圆角16种角组合、TextArea和Button wave证据保持在 [Windows圆角与波纹](../windows/README.md)，其中独立rounded EXE/readback仍为先前通过的版本；本次更新Gallery smoke/Button与当前Gallery EXE身份。两个证据集的verify-only均通过。平台通用99/99 Debug/Release结果见 [共同合同](../common/selection-feedback.md)。
