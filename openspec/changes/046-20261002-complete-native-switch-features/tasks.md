# Tasks

## 1. 平台通用：状态内容与焦点 API

- [x] 1.1 实现 checked/unchecked retained slots、稳定内容宽度、文字/图标裁剪、direction、SwitchRef/autoFocus/onClick 和被动内容回滚；同步 docs/switch.md；在 Windows windows-msvc-headless Debug/Release 运行 selection、Switch public API 与组件合同测试，覆盖控制模式、回调销毁、ref 生命周期/跨线程、内容/方向/窄约束且 Checkbox/Radio 不回退。

## 2. 平台通用：主题与有限反馈

- [x] 2.1 补齐 inner margins/handleShadow/wave tokens、opacityLoading、按压伸展和有限 wave，覆盖 motion/取消/idle/清理，更新 Theme identity/JSON/goldens 与 docs/switch.md；Windows windows-msvc-headless Debug/Release 运行 selection/animation/theme/token 合同测试并验证旧 golden 字段保留。

## 3. 平台通用：Gallery 与集成

- [x] 3.1 集成文字/图标/RTL/ref/尺寸/主题样例和 native implemented 证据，更新生成目录与收尾清单；Windows windows-msvc-headless Debug/Release 完整 build/CTest（含 Core 边界 guards），native Gallery/catalog/字体合同测试；运行 clang-format 22、format-code.py --check、openspec doctor --json、全量 strict validation、git diff --check。

## 4. 原生平台验收

### Windows

- [x] 4.1 windows-msvc Debug/Release build/受影响 CTest，运行真实 SDL/D3D12/DXIL 窗口的内容/图标/RTL/Space/pointer/ref/loading/disabled/wave/resize/失活与销毁验收；系统 DPI 和 1/1.25/1.5/2 render scale，保存 GPU readback PNG、日志与机器可核验结果；检查静止后没有 deadline/持续提交。

### Linux

- [ ] 4.2 在实际 Linux 原生机器完成窗口系统/GPU/系统字体/输入与缩放验收，独立保存日志和截图；不重复平台通用逻辑，不使用 Windows 证据勾选。
