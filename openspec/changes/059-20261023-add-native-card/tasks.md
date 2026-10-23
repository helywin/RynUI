# Tasks

## 1. 规划（平台通用）

- [x] 1.1 核对原生容器/retained surface/语义 slot 与锁定 Card 6.6.5，完成 proposal/spec/design/tasks，doctor/strict/diff check通过后独立提交并立即 push、核对 SHA。

## 2. API 与 Token（平台通用）

- [x] 2.1 实现 Card typed Props/slots/枚举及 Card Token、继承/reset/component seed/algorithm/override、四组 dirty identity、hash/序列化/golden；补公开 API 与默认值/无关 Token 保持/局部失效/非法配置原子拒绝测试和使用文档，windows-msvc-headless Debug/Release 受影响测试、格式/规格检查通过后提交并立即 push、核对 SHA。

## 3. 原生内容容器（平台通用）

- [x] 3.1 接入 retained Card、五区域布局、两尺寸/两变体、hover 祖先识别、静态 loading 分支与焦点清理；补窄宽度/空标题/富 slots、嵌套 Button/Input/Select、loading 恢复编辑值/identity、局部性/idle/dispose 回归与文档；windows-msvc-headless Debug/Release 完整 CTest、格式/规格检查通过，记录平台/preset/evidence后提交并立即 push、核对 SHA。

## 4. Gallery 与共同集成（平台通用）

- [x] 4.1 接入真实 Card 基础/标题/extra/cover/actions/小号/borderless/hover/loading/局部主题样例与切换操作，更新 supported/missing scope/catalog、README；catalog generator check、受影响 Gallery Recording 合同及格式/规格校验通过，记录实际平台/preset/evidence后提交并立即 push、核对 SHA。

## 5. 原生平台验收

### Windows

- [x] 5.1 windows-msvc Debug/Release MSVC x64 构建与受影响 native tests；实际 D3D12/DXIL 窗口验证 Light/Dark、系统与1/1.25/1.5/2 scale、子控件输入/操作、hover/loading/恢复/窄宽度/resize/idle/dispose，核对 PNG/log/字体/driver/exit/EXE hash，独立记录 evidence、提交并立即 push、核对 SHA；自动输入不替代人工系统IME。

### Linux

- [ ] 5.2 实际 Linux linux-native Debug/Release 构建与受影响 native tests、Vulkan/SPIR-V/系统字体/输入/缩放/resize 实际窗口验收，独立记录 evidence、提交并立即 push、核对 SHA；不重复 common、不使用 Windows 或 WSLg 替代。
