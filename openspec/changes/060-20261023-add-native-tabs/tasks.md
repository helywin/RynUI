# Tasks

## 1. 规划（平台通用）

- [x] 1.1 核对固定6.6.5 Tabs和现有窗口/输入/分支/clip边界，完成proposal/spec/design/tasks；doctor/strict/diff check通过后独立提交并立即push、核对SHA。

## 2. API、选择模型与Token（平台通用）

- [x] 2.1 实现typed TabsProps/TabItem/TabContent、选择/禁用/导航模型与Tabs Token、继承/reset/component算法/覆盖、dirty分组、hash/序列化/golden；补空/重复key、控制冲突、未知key、拒绝回写、disabled导航、旧Token保持/局部性/非法配置原子拒绝与API编译测试和文档，windows-msvc-headless Debug/Release受影响测试、格式/规格校验通过后记录evidence、提交并立即push、核对SHA。

## 3. 保留面板与真实导航（平台通用）

- [x] 3.1 接入retained header/所有预建面板、line/card、三尺寸/top/bottom/centered、单Tab入口/keyboard/pointer/callback、overflow箭头/wheel/显露/clip/resize与旧面板输入/窗口层清理；补真实Input/Select草稿恢复、受控拒绝、禁用/焦点/捕获/回调卸载、label更新/identity/字形滚动局部性/idle/dispose测试和文档；windows-msvc-headless Debug/Release完整CTest、格式/规格检查通过后记录平台/preset/evidence并提交、立即push、核对SHA。

## 4. Gallery与共同集成（平台通用）

- [ ] 4.1 接入真实Tabs基础/受控/禁用/label/两类型/三尺寸/上下/居中/溢出/保留编辑/局部主题样例，更新supported/missing scope/catalog、README及常用组件清单；catalog generator check、受影响Gallery Recording合同和格式/规格检查通过，记录实际平台/preset/evidence后提交并立即push、核对SHA。

## 5. 原生平台验收

### Windows

- [ ] 5.1 windows-msvc Debug/Release MSVC x64完整构建及受影响native tests；实际D3D12/DXIL窗口验证Light/Dark、system/1/1.25/1.5/2 scale、标签键盘/指针/溢出/resize、面板Input自动IME/恢复与Select清理、idle/dispose，核验PNG/log/系统字体/driver/exit/EXE hash，独立记录evidence后提交并立即push、核对SHA；不以自动事件代替人工系统IME。

### Linux

- [ ] 5.2 实际Linux linux-native Debug/Release构建及受影响native tests、Vulkan/SPIR-V/系统字体/输入/缩放/resize真实窗口验收，独立记录evidence、提交并立即push、核对SHA；不重复common，不使用Windows或WSLg替代。
