# Tasks

## 1. 平台通用：日期值、模型与 API

- [x] 1.1 实现有效公历日期/可空选择、ISO 解析与格式、范围及月历导航模型、typed Props/ref/footer API，并在 docs/date-picker.md 记录支持范围、UTC 缺省面板及限制；使用 windows-msvc-headless 的 MSVC Ninja Multi-Config 构建并通过 Debug/Release model/public API 测试，覆盖世纪闰年、1/9999 年边界、非法输入、禁用规则、受控拒绝与更新。

## 2. 平台通用：Theme 与原生保留控件

- [x] 2.1 实现 DatePicker Theme 全链路及保留 Input/月历/typed footer host，补齐 API 用例文档与实际 component/Theme 测试；通过 Debug/Release focused tests 覆盖指针/键盘/输入/清空、范围导航、普通更新保留草稿与身份、受控开合、Modal/Spin/隐藏及子 popup、有限 glyph/surface/hit clip 和回调销毁；生成五份 fresh golden 并证明旧字段除 identity 外保持。

## 3. 平台通用：集成收口

- [x] 3.1 运行受影响的 Input、Button、Theme、popup/Modal/Spin 共同回归和 Core/renderer 边界检查所需的实际 headless 构建，确认新增组件没有持续空闲帧/无界搜索；完成 API 文档、验收证据及首版限制，通过 clang-format 22.x、doctor、strict validation、git diff --check 后收口。不得把共同测试或未执行的 Gallery/GPU/Linux/系统 IME 写为平台验收通过。
