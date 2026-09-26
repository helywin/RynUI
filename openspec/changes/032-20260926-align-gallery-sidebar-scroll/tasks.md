# Tasks

## 1. 规划与参照

- [x] 1.1 固定官方网页参照、范围、架构边界与验收；运行本 change strict、全仓 strict、可用 doctor、`git diff --check`，以 `docs:` 提交

## 2. 平台通用滚动区与装饰组件

- [x] 2.1 抽取 Gallery 内部滚动区状态，保留文档锚点、clamp 与 generation 合同；增加独立 offset 和子树隔离单元测试，在 Windows MSVC Debug 定向 CTest 后以 `feat:` 提交
- [x] 2.2 先完成 Gallery 滚动条/顶栏装饰组件及 Theme 样式，单元测试覆盖轨道滑块几何、拖动映射与短内容，在 Windows MSVC Debug 定向 CTest 后以 `feat:` 提交

## 3. 平台通用导航与布局

- [x] 3.1 把导航改为纵向文档入口、类别标题及目录条目，并加入固定顶部站点栏；headless 测试覆盖目录顺序、绘制层次与挂载身份，以 `feat:` 提交
- [x] 3.2 宽窗口按指针列路由滚轮和滚动条操作，正文仅平移正文根，左栏单独滚动；窄窗口保持连续可达；headless frame/viewport 测试覆盖跳转、滚动隔离、拖动和 resize，以 `fix:` 提交

## 4. Windows 真实窗口验收

- [x] 4.1 正式 Windows MSVC Release 构建，运行真实 D3D12 滚轮、拖动/轨道、主题与输入验收，记录两列终态、首帧/滚动 CPU、上传与 draw；通过后以 `test:` 提交

## 5. 集成验收

- [x] 5.1 运行完整 CTest、本 change strict、全仓 strict、可用 doctor、`git diff --check`；记录未通过项，满足门槛后以 `test:` 提交

## 6. 用户反馈修正：拖动稳定性与排版

- [x] 6.0 修复 Windows MSVC 诊断语言引起的 Ninja 头文件依赖丢失，清理重建 Debug/Release，验证依赖记录与增量重编译后以 `fix:` 提交

- [x] 6.1 复现持续拖动至正文中段的问题，修复根因；增加经过中间位置、往返及结束拖动的回归，运行 Windows MSVC Debug 定向 CTest 与 Release D3D12 连续拖动验收，以 `fix:` 提交
- [x] 6.2 按官方组件页重新整理左栏行高、对齐、正文宽度、标题层级及内容间距；同步布局与滚动几何来源，运行 Windows MSVC Debug 布局/viewport 定向 CTest，以 `fix:` 提交
- [ ] 6.3 在 Windows MSVC Release 真实窗口检查首页、中段和窄窗口排版，复验拖动、滚动与输入；运行本 change strict、全仓 strict、可用 doctor、`git diff --check` 并记录证据，以 `test:` 提交
