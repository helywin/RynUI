# Tasks

## 1. 规划（平台通用）

- [x] 1.1 核对既有输入/窗口层/scroll/Theme 边界与 Ant Design 6.6.5，记录常用组件优先级，创建完整规划并通过 doctor/strict/diff check，以英文 Conventional Commit 提交并立即 push、核对远端 SHA。

## 2. Token、公开 API 与选择模型（平台通用）

- [x] 2.1 实现 typed SelectValue/SelectOption/Props/slots/ref 与选择/搜索模型，加入 Select Token、override/算法/继承、dirty identity、snapshot/hash/序列化/golden；补 String/int64/空值/未知值、重复选项原子拒绝、controlled 拒绝与 Unicode 搜索、Token 域与旧值保持测试及阶段文档，windows-msvc-headless Debug/Release 受影响 tests、格式/规格检查通过后提交并立即 push、验证 SHA。

## 3. 完整单选交互（平台通用）

- [x] 3.1 接入预建 Input、独立窗口层菜单与 retained 行、单 Tab 入口、pointer/keyboard/IME/外部关闭、滚动和边缘定位、disabled/loading/clear/empty/open 控制与生命周期；补真实组件、焦点/捕获/回调销毁、TextArea scroll 未变、identity/局部性/idle 回归及使用文档，windows-msvc-headless Debug/Release 完整 CTest 与格式/规格检查通过后记录 evidence、提交并立即 push、核对 SHA。

## 4. Gallery 与公共集成（平台通用）

- [ ] 4.1 接入单选/搜索/数值/禁用/clear/loading/空结果/三尺寸/四变体/status/controlled 的真实 Gallery 样例，更新 supported/missing scope/catalog、README 与文档；catalog check、受影响 Gallery Recording 合同和最终共同集成通过，记录平台/preset/evidence，提交并立即 push、核对 SHA。

## 5. 原生验收

### Windows

- [ ] 5.1 windows-msvc Debug/Release MSVC x64 构建与受影响 native tests，实际 D3D12/DXIL 窗口验证 Light/Dark、系统与1/1.25/1.5/2 scale、选择/搜索/自动IME事件/keyboard/scroll/边缘/resize/失活/idle/dispose，保存并核对 PNG/log/字体/driver/exit/EXE hash；独立提交并立即 push、核对 SHA，自动输入不替代人工系统IME。

### Linux

- [ ] 5.2 实际 Linux linux-native Debug/Release 构建与受影响 native tests，完成对应 Vulkan/SPIR-V/系统字体/输入/缩放/resize 的实际窗口验收，独立记录 evidence 并提交、push、核对 SHA；不重复common，不使用Windows或WSLg替代。
