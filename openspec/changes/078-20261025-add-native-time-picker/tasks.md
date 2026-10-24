# Tasks

## 1. 平台通用：时间值、模型与 API

- [x] 1.1 实现有效时间值、可空选择、严格两种格式、有限步长/范围/禁用、待提交与受控模型、typed Props/ref/footer API；编写 docs/time-picker.md 的模型合同与首版限制，在 windows-msvc-headless / MSVC / Ninja Multi-Config 的 Debug、Release 通过 model/public API tests，覆盖 00/23:59:59、非法/未完成输入、分钟精度、全禁用、受控拒绝和取消。

## 2. 平台通用：保留面板与 Theme

- [x] 2.1 实现保留 Input、真实滚动时间列、确认/即时模式、键盘/pointer/wheel/clear、typed footer、完整 Theme 链路与实际 API 示例；Debug、Release component/Theme focused tests 验证真实值/草稿/身份、三尺寸/变体、有限 glyph/surface/hit clip、普通和算法派生非法 token 发布前拒绝、关闭算法忽略局部 seed；生成五份 fresh goldens 并对基线证明旧字段除 identity 外保持。

## 3. 平台通用：集成收口

- [x] 3.1 运行受影响 Input、Button、Theme、Modal/Spin 与 popup 共同回归，包含真实 footer Table/Select、父子/重叠 popup、隐藏抑制、初始开合、回调销毁和失效 ref，确认 Core/renderer 守卫及实际 headless 构建通过、空闲无持续帧/无界搜索；完成 API 文档和准确证据，通过 clang-format 22.x、doctor、strict validation、diff check。未运行的 Gallery、原生 GPU、Linux 和系统 IME 不记为通过。

## 4. 平台通用：合并已验收 DatePicker / Form 基线

- [x] 4.1 在本阶段干净提交后显式 merge `0fdefbae9d5fb67422d6a67471019d5e3227dac1`，并在 R1 已验证提交 `21c863c` 后显式 merge Date reset worker `0e0f11f7cb903f6b629ce02d5561621fbdb5cee4`；保留 Date/Form/Time 的 API、Theme/IIFE/identity、宿主与内容先行的 Date/Time/Dropdown 边界链，合并后只有一个共享 reset hook，保留 focused 诊断。以 `78c4adf` 和 `0fdefbae` 双向比较五份 fresh goldens 的既有字段，运行 Debug/Release scoped combined tests 与新增 Date/Time review tests，记录实际复现、工具检查和独立本地 merge commit；不 push，Gallery 仅随干净父提交进入，不自行修改。

## 5. 平台通用：Astra R1 五项 P2 修复

- [x] 5.1 先复现受控 open 关闭请求被私自隐藏；区分普通关闭与强制边界 suppression，验证 Escape/OK/Cancel/outside 的接受、拒绝及重试。
- [x] 5.2 先复现空值即时模式激活合法 00 行未发布、已选分钟行未切换 active column；验证真实 pointer、后续键盘、禁用行与受控拒绝。
- [x] 5.3 先复现同一接受值的实际 String Form adapter reset 保留未提交草稿；通过最小内部 owner reset hook 清理 model/显示/preedit/待确认值，保留普通 validation/help/Theme 更新的草稿与失效 generation 安全；验证读取 authority 时同步销毁 owner 后安全返回。
- [x] 5.4 先复现关闭后 stale focus tracking 使程序化新 open 周期立即关闭；验证重复程序化打开、defaultOpen 与边界恢复。
- [x] 5.5 先复现 footer Input Escape 缺失 onBlur；保留正常 deferred focus/blur 交付，验证 NORMAL motion 的内部焦点、session/caret 清理、selector restore、同步销毁/重新打开与 generation。

## 6. 平台通用：Astra R2 行激活焦点回调生命周期

- [x] 6.1 先用真实 pointer 行点击与 footer Input onBlur 复现 focus 转移后访问旧 owner/失效提交；在 choose 的 focus 回调后重取同一 generation，校验当前 scope、visible、disabled、窗口和输入边界，使用新的 model/needConfirm。覆盖即时/确认模式销毁、复用 ref 创建新 generation，以及存活 owner 的关闭、禁用、Spin/Modal/窗口/祖先阻断、受控值/rules/needConfirm 更新；保留全部 R1/Date reset 回归，定向 Debug/Release、fresh goldens 与工具检查通过后本地提交，不运行无关完整矩阵或合入新 main。
