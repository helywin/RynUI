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
