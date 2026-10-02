# Tasks

## 1. 规划（平台通用）

- [x] 1.1 完成 proposal/spec/design/tasks 与原生组件收尾清单，doctor、full strict、diff check 通过后保存证据并提交；按用户授权立即 apply。

## 2. 窗口浮层基础（平台通用）

- [ ] 2.1 实现窗口层子树稳定顺序、布局后/文字前浮层定位、共同 ancestor input 和 Escape 预处理；补层次/隐藏/销毁/输入回归合同，修复 Gallery 陈旧状态数量断言，Windows/MSVC headless Debug/Release 完整 CTest、格式/doctor/strict/diff 后记录证据并独立提交。

## 3. Tooltip 整体交付（平台通用）

- [ ] 3.1 完成 typed API/slot、受控/非受控、hover/focus delay/cancel/Escape/reentrant callback、十二种定位/flip/shift/arrow、Theme token/algorithm/继承/分阶段失效、同帧文本和 retained scene、Gallery 与使用文档；补 public API、数值/输入/生命周期/idle/scene/主题负例，Windows/MSVC headless Debug/Release 完整 CTest 和格式/doctor/strict/diff 后保存证据并提交。

## 4. Windows 原生验收

- [ ] 4.1 windows-msvc Debug/Release 构建及完整 CTest；实际 D3D12 Tooltip hover/focus/Escape/disabled child/受控/边缘/长 CJK/arrow/Default/Dark/Compact/resize/四档 scale 运行，保存截图、诊断、SHA256、退出码，doctor/strict/diff 后独立提交。

## 5. Linux 原生验收

- [ ] 5.1 实际 Linux GCC/Clang native preset 构建，验证 Vulkan/SPIR-V/Fontconfig/Wayland Tooltip 输入/定位/scale/resize，独立保存 evidence 并提交；不重复通用合同，不以 Windows 结果代替。
