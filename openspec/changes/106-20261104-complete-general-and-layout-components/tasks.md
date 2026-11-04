# Tasks

## 1. 通用：FloatButton

- [x] 1.1 实现 typed FloatButton/Group/BackTop、主题、输入与生命周期，新增合同测试覆盖点击/悬停/受控、禁用/销毁、定位拖动和返回顶部。
- [x] 1.2 接入 FloatButton 独立页面与说明；运行 Linux HEADLESS Debug/Release 相关测试和目录校验，记录结果后提交。

## 2. 通用：Typography 与分类收口

- [ ] 2.1 实现 typed rich runs、真实混合样式排版与 HTML 复制；测试 Unicode、换行、省略、主题、反应更新和剪贴板失败。
- [ ] 2.2 更新 Typography 页面和通用支持清单；运行受影响 Typography/Text/clipboard 自动测试，构建 Gallery 并记录本机结果后提交。

## 3. 布局：Grid

- [ ] 3.1 实现 Row/Col、共享响应断点、跨度/偏移/排序/位移/flex、双轴 gutter、RTL/隐藏；合同测试覆盖窗口变化与稳定身份。
- [ ] 3.2 接入 Grid 页面与文档；运行相关 HEADLESS Debug/Release 测试和目录校验后提交。

## 4. 布局：Layout

- [ ] 4.1 实现嵌套 Layout/Header/Footer/Content/Sider、主题、响应式/受控折叠与触发器；自动测试验证零宽度、焦点和生命周期。
- [ ] 4.2 接入 Layout 页面与文档；运行相关 HEADLESS Debug/Release 测试和目录校验后提交。

## 5. 布局：Masonry

- [ ] 5.1 实现响应式 columns/gutter、稳定 keyed items、指定列/高度、真实尺寸重排与变化回调；自动测试覆盖动态内容、无效值和销毁。
- [ ] 5.2 接入 Masonry 页面与文档；运行相关 HEADLESS Debug/Release 测试和目录校验后提交。

## 6. 布局：Splitter

- [ ] 6.1 实现多面板尺寸求解、指针/键盘调整、约束、折叠/恢复、受控值、lazy、事件与隐藏生命周期；自动测试覆盖嵌套、取消和销毁。
- [ ] 6.2 接入 Splitter 页面与文档；运行相关 HEADLESS Debug/Release 测试和目录校验后提交。

## 7. 集成与文档校验

- [ ] 7.1 更新完整支持范围、架构接口和精简验收；运行受影响公共头/依赖合同、格式检查、OpenSpec doctor、strict 全部文档校验及 git diff --check。
- [ ] 7.2 构建 Linux 原生 Gallery；通过私有后台 Wayland 运行新增组件的有界自动输入验收，记录 GPU/输入实际结果，不启动用户桌面窗口。

## 8. Windows 自动集成

- [ ] 8.1 在实际 Windows MSVC HEADLESS Debug/Release 构建新增 target 并运行受影响测试，记录工具链与结果。
- [ ] 8.2 在实际 Windows 原生环境执行富文本剪贴板及新增组件窗口自动集成，记录对应平台结果。

平台通用逻辑以本次 Linux preset 证据验收一次。Windows 项只追踪 MSVC/原生平台集成，不重复要求通用合同矩阵。人工体验建议见验收说明，不属于实施任务。
