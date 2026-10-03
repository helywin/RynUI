# Tasks

## 1. 平台通用：圆角 coverage

- [x] 1.1 修正零 blur coverage、inset surface mask 与 AA bounds，同步 logical/packed reference、HLSL/source lock，补曲面边界/平移/fractional DPI/ancestor clip/四象限回归和 renderer 文档；windows-msvc-headless Debug/Release 的 rounded_effect math/store/scene/gpu/resources/allocation、Core boundary 通过，windows-msvc shader 生成/部署合同通过，format-code/doctor/strict validate/diff check 后记录实际 preset/evidence 并提交。

## 2. 平台通用：官方 Button wave

- [x] 2.1 加入独立400ms扩展与2000ms淡出通道、motionEaseOutCirc、实际颜色与外扩色带，修正默认 token/override 语义，补阶段时序、颜色fallback、重启/销毁/失活/策略与 retained 回归及官方参考文档；windows-msvc-headless Debug/Release 完整CTest（含allocation/idle）、相关Gallery frame/catalog通过，格式/规格校验和evidence后提交。

## 3. 原生分平台验收

### Windows

- [x] 3.1 windows-msvc Debug/Release构建专用真实D3D12/DXIL窗口，在系统/1/1.25/1.5/2缩放捕获浅/深色圆角fill、细border、inset、Quad对照、TextArea截图复现及Button 0/100/400/1000/2000ms wave、resize/策略取消/idle/dispose；GPU readback逐像素与参考在声明容差内匹配且curve存在分数coverage、四象限无接缝，实际图像目视审核，native smoke/shader合同与PNG/log/EXE hashes核验通过，独立提交Windows evidence。

### Linux

- [ ] 3.2 实际Linux机器linux-native Debug/Release运行对应Vulkan/SPIR-V shader/window/DPI矩阵和像素检查、resize/输入/idle/dispose，记录该平台独立evidence并提交；不重复共同逻辑，不用Windows结果代替。

## 4. 平台通用：填充接缝与 Switch 手柄

- [x] 4.1 修正 RadioButton border-box 填充与同色边缘合成；Switch 位置与逻辑两端30%伸长同时连续过渡，补正常/reduced/motion-off、双尺寸/LTR/RTL、快速反向、受控值、指针/键盘/取消/销毁/idle及内容局部性回归和参考文档；windows-msvc-headless Debug/Release完整CTest、format-code/doctor/strict validate/diff check通过，记录preset/evidence并提交。

## 5. 补充分平台验收

### Windows

- [x] 5.1 windows-msvc Debug/Release真实D3D12窗口捕获Switch按压/释放中间帧与双向位置、RadioButton Solid填充（含混合圆角、浅/深、LTR/RTL、系统/1/1.25/1.5/2 scale），实际像素与连续几何检查、native smoke及图像目视审核通过，记录截图/log/hash并独立提交。

### Linux

- [ ] 5.2 实际Linux机器linux-native Debug/Release完成对应Switch中间帧、RadioButton填充的Vulkan/SPIR-V窗口矩阵及检查，记录独立evidence并提交；不重复共同逻辑。
