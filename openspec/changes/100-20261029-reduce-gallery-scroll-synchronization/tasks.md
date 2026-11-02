# Tasks

## 1. 平台通用同步优化

- [x] 1.1 剔除 Input 不可见字形层并回归屏幕外更新、恢复 glyph/clip、选择与保留身份；官方 MSVC HEADLESS Debug/Release 运行相关 Input/Text/编辑代表，记录实际结果。
- [x] 1.2 减少纯滚动锚点扫描并修正采样计数边界；保留布局变化导航与采样区间零额外同步检查，以 Gallery contracts 和相关 Recording 导航回归验证。
- [x] 1.3 消除 Button 完全透明 loading 分段的无效几何更新，给 Gallery 的 Quad 小块上传设置有界合并，并让 Glyph buffer 增长复用容量；验证恢复、字节保留、有界范围与失败重试，运行相关 Button/动画/资源上传 Debug/Release 回归和原生测量，并校正性能分项标签。

## 2. Windows 原生滚动

- [x] 2.1 同配置新构建 MSVC Release Gallery，在用户授权下短暂自动滚动并退出；记录前后平均/p95、CPU/上传量、配置及限制，完成格式、OpenSpec 1.14 doctor/strict、diff 检查和独立审查后提交推送并核远端 SHA。不以性能数字替代 Linux/IME/AT 验收。

## 3. Linux 滚轮回归（2026-11-02）

- [x] 3.1 Astra xhigh 定位滚轮输入路径；提前拒绝几何落空的 hit records，保持实际候选的动态祖先/屏蔽/边界检查，通过 hit-test 回归及密集 benchmark。实际 Linux GCC Release 的完整 Gallery 滚轮路由两轮各 240 次通过，scrollbar 路由通过，记录分发耗时及边界，不再用直接 scroll_by 代表滚轮。
- [ ] 3.2 用户在已修复版本的“交互示例”等长页面手动滚动鼠标滚轮，确认连续滚动流畅、没有原先的明显停顿，并与拖动滚动条比较；收到用户实际结果后再勾选。

Linux 自动结果与未验边界见 [验收摘要](../../../docs/validation/linux-native-20261102.md)。自动窗口测试改为独立后台 Wayland，禁止回退到用户当前桌面；后台动画读回超时保留未通过。
