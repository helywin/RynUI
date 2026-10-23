# Tasks

## 1. 规划（平台通用）

- [x] 1.1 核对固定6.6.5 Modal和现有窗口层/输入/焦点/分发顺序，完成proposal/spec/design/tasks；doctor、strict和diff check通过后独立提交、立即push并核对SHA。

## 2. API、生命周期模型和Token（平台通用）

- [x] 2.1 实现typed ModalProps/ModalContent/ModalSlots/Mask/取消原因、受控请求和显示转换模型，以及Modal Token继承/reset/component算法/覆盖、dirty分组/hash/序列化/golden；补加载/取消入口/父级拒绝/重复状态/非法值原子拒绝、旧Token不变与API测试和阶段文档；windows-msvc-headless Debug/Release受影响测试、格式/规格检查通过后记录evidence、提交并立即push、核对SHA，声明不计作实际对话框完成。

## 3. 模态输入边界（平台通用）

- [x] 3.1 实现内部逻辑子树交互边界并接入pointer/capture/focus/ref/session、背景滚轮和外部Select/Tooltip抑制；保持绘制及控件自身disabled值，边界内子窗口层继续可用；补无边界兼容、背景引用聚焦/Tab/捕获取消、动态挂载/失效root、子层与IME Escape优先测试及内部文档；windows-msvc-headless Debug/Release受影响测试、格式/规格检查通过后记录evidence、提交并立即push、核对SHA。

## 4. 保留对话框和视口交互（平台通用）

- [x] 4.1 接入Modal host、正常流零占位/遮罩和独立窗口层、保留header/body/footer/close/default Button、自定义slots、受控取消确认/加载/关闭通知、最上层与焦点恢复、有限视口和正文滚动/焦点caret显露；补真实Input/TextArea/Select/Tooltip/Tabs、长正文/clip/resize/字形局部性、Tab环绕/无入口、嵌套与失效触发、回调重开/卸载/捕获/窗口失活、identity/idle/dispose测试和API文档；windows-msvc-headless Debug/Release完整CTest、格式/规格检查通过后记录平台/preset/evidence、提交并立即push、核对SHA。

## 5. Gallery集成（平台通用）

- [x] 5.1 接入实际Modal基础/受控拒绝/取消入口/加载/保留编辑/自定义页脚/无页脚/无遮罩/居中/长正文/嵌套/局部主题样例，更新supported/missing scope/catalog、README和常用组件清单；补完整Gallery整页滚动下的打开/编辑/子层/关闭/恢复/背景滚轮合同；generator check、受影响Gallery Recording测试和格式/规格检查通过后记录实际平台/preset/evidence、提交并立即push、核对SHA。

## 6. 原生平台验收

### Windows

- [ ] 6.1 windows-msvc Debug/Release MSVC x64完整构建及受影响平台/SDL/input tests；实际D3D12/DXIL窗口验证Light/Dark、system/1/1.25/1.5/2缩放、遮罩/取消确认/加载、键盘焦点约束与恢复、Input自动composition/草稿、Select/Tooltip Escape、正文滚动/resize/嵌套、idle/dispose；核对PNG/log/系统字体/driver/exit/EXE hash，独立记录evidence后提交并立即push、核对SHA，不以自动事件代替人工系统IME。

### Linux

- [ ] 6.2 实际Linux linux-native Debug/Release构建及受影响平台/input tests、Vulkan/SPIR-V/系统字体/模态输入焦点/缩放/resize真实窗口验收，独立记录evidence后提交并立即push、核对SHA；不重复common、不以Windows或WSLg替代。
