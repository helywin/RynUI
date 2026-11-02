# Tasks

## 1. 共用形状与裁剪（平台通用）

- [ ] 1.1 修复Quad完整AA光栅域和一致物理coverage，保留48-byte ABI；以方/圆角/圆形、多phase及1/1.25/1.5/2倍缩放的实际draw bounds与source-over回归验证，更新renderer合同。
- [ ] 1.2 修复Table后代填充裁剪转换，覆盖surface/content及窗口层转换退出；真实Checkbox/InputNumber/Tag滚动回归验证fill/glyph/hit均受裁剪。
- [ ] 1.3 修复RadioButton共享选中边绘制所有权；竖排false/true/数字和横排/RTL边缘覆盖回归，保留原Solid内部连续性断言。
- [ ] 1.4 修复虚线周长phase、Tooltip四向接缝及Dashboard弧端；按透明度和真实路径coverage验证，保留开口、clip及原断言。

## 2. 文字和编辑几何（平台通用）

- [ ] 2.1 修复确认的共享文字/Icon度量及对齐调用错误；用中英、Icon和Checkbox/Alert/Tree/TreeSelect/Avatar真实布局断言验证，不用固定样例偏移。
- [ ] 2.2 修复OTP空格caret居中和InputNumber尾端动作尺寸/箭头同轴；覆盖值、尺寸、mask、disabled/readonly与retained身份，运行相关编辑回归。
- [ ] 2.3 修复Modal内容间距及Tree checkbox/富行布局；运行对应组件和Gallery反馈入口，核对focus decoration与内容无重叠。

## 3. 状态呈现与逻辑输入（平台通用）

- [ ] 3.1 修复Switch内容/滑块中间态、快速反向、小尺寸RTL及门禁变化；针对性动画/身份/idle回归，核对锁定Ant基线。
- [ ] 3.2 核对并修复Timeline loading图标的尺寸/线宽/完整覆盖及运动；针对性图标和Timeline回归，保留retained内容。
- [ ] 3.3 增加文本编辑区域的逻辑cursor选择，涵盖组合输入及退役/离开恢复；用命中、门禁与生命周期测试验证，Core不依赖SDL。

## 4. Windows 原生验收

- [ ] 4.1 补全可光栅的系统emoji fallback，运行default_font_chain和实际shape/raster回归，保持正文Unicode/字素统计与无字体文件普通启动。
- [ ] 4.2 接入并验证SDL系统I-beam资源生命周期；用Release窗口检查输入区、非输入区及退役/退出行为。
- [ ] 4.3 正式windows-msvc Release重建最终Gallery（含内嵌shader），短暂打开检查本批25项场景；只记录实测DPI/平台，不代表Linux或系统IME/AT。

## 5. 集成收口（平台通用）

- [ ] 5.1 受影响targets在windows-msvc-headless Debug/Release构建和对应CTest通过；format-code --check、OpenSpec doctor/strict、diff检查通过，简短记录实际数量和边界。
- [ ] 5.2 将25条报告逐项映射到修复/实测结论，复核共用路径与现有测试未弱化；各阶段commit后安全fetch/rebase/push并核对远端SHA，提供新版Gallery路径。

Linux原生GPU、系统字体与系统cursor不属于本机Windows验收；共享逻辑测试不要求在Linux重复。后续在Linux真实机器开展原生复核时另设独立任务，不用Windows结果代验。
