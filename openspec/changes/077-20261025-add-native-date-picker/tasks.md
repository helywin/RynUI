# Tasks

## 1. 平台通用：日期值、模型与 API

- [x] 1.1 实现有效公历日期/可空选择、ISO 解析与格式、范围及月历导航模型、typed Props/ref/footer API，并在 docs/date-picker.md 记录支持范围、UTC 缺省面板及限制；使用 windows-msvc-headless 的 MSVC Ninja Multi-Config 构建并通过 Debug/Release model/public API 测试，覆盖世纪闰年、1/9999 年边界、非法输入、禁用规则、受控拒绝与更新。

## 2. 平台通用：Theme 与原生保留控件

- [x] 2.1 实现 DatePicker Theme 全链路及保留 Input/月历/typed footer host，补齐 API 用例文档与实际 component/Theme 测试；通过 Debug/Release focused tests 覆盖指针/键盘/输入/清空、范围导航、普通更新保留草稿与身份、受控开合、Modal/Spin/隐藏及子 popup、有限 glyph/surface/hit clip 和回调销毁；生成五份 fresh golden 并证明旧字段除 identity 外保持。

## 3. 平台通用：集成收口

- [x] 3.1 运行受影响的 Input、Button、Theme、popup/Modal/Spin 共同回归和 Core/renderer 边界检查所需的实际 headless 构建，确认新增组件没有持续空闲帧/无界搜索；完成 API 文档、验收证据及首版限制，通过 clang-format 22.x、doctor、strict validation、git diff --check 后收口。不得把共同测试或未执行的 Gallery/GPU/Linux/系统 IME 写为平台验收通过。

## 4. 父线程 main 集成

- [x] 4.1 将 worker 1ca5416 显式合并到当前 main，保留 Form 与 076 导航/079 规划；在父构建树通过组合 Debug/Release 39 项、fresh golden 双基线旧字段与既有 IIFE 比较、目录 generator/contracts/inventory 和新构建 Gallery 启动 smoke，更新 partial 支持清单与准确证据，记录独立 Astra R2 的真实结论，通过格式/doctor/strict/diff 后提交推送 main 并核对远端 SHA，不推送 t3 分支。

父集成提交 `0fdefbae9d5fb67422d6a67471019d5e3227dac1` 已推送，`git ls-remote origin refs/heads/main` 与本地 HEAD 相等，`t3/*` 远端查询为空。Astra R2 CLEAN、父 Debug/Release 各 39/39 与目录/启动证据见 [main-integration.md](evidence/main-integration.md)。该轮原有四项已完成；079 日期 Gallery 发现的聚焦 reset 问题由下列增量任务跟踪。

## 5. 平台通用：聚焦 Form reset 的复合草稿

- [x] 5.1 在 verified main `b28bd952` 的独立 worktree 中先复现 String Form 同值 reset 保留 DatePicker 草稿的问题；实现内部 Input owner reset hook，在 editor 重读 authority 前恢复 DatePicker 已接受值并清除 dirty draft。覆盖直接聚焦 reset、合法/不完整未提交草稿、非空初值、受控拒绝、preedit 与普通更新、身份/焦点、旧 ref 代际和销毁；通过官方 MSVC headless Debug/Release 的 Date/Form/Input/InputNumber/TextArea/Selection/Focus/Modal/Spin 定向回归、五份 fresh golden 不变、format 22/doctor/strict/diff，记录真实失败与通过证据后本地提交，不修改 Gallery 或导入 TimePicker WIP。

聚焦 reset 的真实失败、修复与最终 Debug/Release 各 33/33、五份金样完全不变及平台边界见 [form-reset.md](evidence/form-reset.md)。本增量不勾选 079 Gallery 任务；由父线程与独立 Gallery worker 复验交付。
