# Tasks

## 1. 规划（平台通用）

- [x] 1.1 完成 proposal/spec/design/tasks，验证 doctor、全量 strict 和 diff check，保存规划证据并按英文 Conventional Commit 提交；按用户授权立即进入 apply。

## 2. Slider 整体实现（平台通用）

- [ ] 2.1 实现 typed 单值/范围 API、有限数值/小数 step/动态 limits、controlled、mouse/touch capture/cancel、双 thumb focus/keyboard/reverse/vertical、token 继承/算法/分阶段失效及 retained logical scene；补公开 API、数值/交互/lifecycle/重入负例、Recording 局部/idle 合同、Gallery 与使用文档；在 Windows/MSVC windows-msvc-headless Debug/Release 完整 CTest 验证，doctor/strict/diff check 后保存通用证据并提交。

## 3. Windows 原生验收

- [ ] 3.1 使用 windows-msvc-debug/release 构建 native 目标，验证 SDL keys/scene/frame/组件合同，实际 Slider 单值/范围/vertical/reverse/disabled/键盘/拖动/scale/resize 窗口验收，记录 GPU、截图、运行结果及当前可执行文件 SHA256；doctor/strict/diff check 后独立提交。

## 4. Linux 原生验收

- [ ] 4.1 在实际 Linux native preset 验证新增 keys normalization 与 Vulkan Slider 窗口/输入/DPI/resize，保存独立 Linux evidence 并提交；不重复平台通用合同，Windows 结果不替代此项。
