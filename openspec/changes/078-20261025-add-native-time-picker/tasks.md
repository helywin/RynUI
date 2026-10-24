# Tasks

## 1. 平台通用：时间值、模型与 API

- [x] 1.1 实现有效时间值、可空选择、严格两种格式、有限步长/范围/禁用、待提交与受控模型、typed Props/ref/footer API；编写 docs/time-picker.md 的模型合同与首版限制，在 windows-msvc-headless / MSVC / Ninja Multi-Config 的 Debug、Release 通过 model/public API tests，覆盖 00/23:59:59、非法/未完成输入、分钟精度、全禁用、受控拒绝和取消。

## 2. 平台通用：保留面板与 Theme

- [x] 2.1 实现保留 Input、真实滚动时间列、确认/即时模式、键盘/pointer/wheel/clear、typed footer、完整 Theme 链路与实际 API 示例；Debug、Release component/Theme focused tests 验证真实值/草稿/身份、三尺寸/变体、有限 glyph/surface/hit clip、普通和算法派生非法 token 发布前拒绝、关闭算法忽略局部 seed；生成五份 fresh goldens 并对基线证明旧字段除 identity 外保持。

## 3. 平台通用：集成收口

- [ ] 3.1 运行受影响 Input、Button、Theme、Modal/Spin 与 popup 共同回归，包含真实 footer Table/Select、父子/重叠 popup、隐藏抑制、初始开合、回调销毁和失效 ref，确认 Core/renderer 守卫及实际 headless 构建通过、空闲无持续帧/无界搜索；完成 API 文档和准确证据，通过 clang-format 22.x、doctor、strict validation、diff check。未运行的 Gallery、原生 GPU、Linux 和系统 IME 不记为通过。
