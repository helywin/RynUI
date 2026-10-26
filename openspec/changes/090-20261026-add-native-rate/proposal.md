# Proposal

## Why

RynUI 缺少可交互的原生评分组件。新增 retained Rate，将锁定 Ant Design 6.6.5 的评分、尺寸和颜色合同映射到现有原生输入与 logical scene。

## What Changes

- 新增 typed `RateProps`、文本/图标 `RateCharacter` 与 reactive count/value/状态/size，提供单一 Tab 入口、鼠标预览、半选、清空、键盘编辑和逐项 Tooltip。
- 新增 Rate Theme 全链，保留旧派生块、旧 TokenIdentity 和旧 snapshot 字段；发布前验证派生尺寸与字体。
- 新增 `docs/rate.md` 与定向真实事件、glyph/quad/clip、资源与主题测试。
- 非目标：Gallery、目录元数据、GPU ABI、DOM/CSS 移植、任意有状态原生控件作为重复 character、系统 AT 与新平台验收。

## Capabilities

### New Capabilities

- `native-rate`: 原生 retained 评分、交互、裁剪、主题与生命周期。

### Modified Capabilities

无。

## Impact

新增公开 header、component host、Theme token/derivation/runtime、窗口服务注册、CMake 与测试。风险集中于半星实际内容裁剪、受控拒绝、隐藏/输入边界取消、非法主题更新的原子恢复。平台通用逻辑在 Windows MSVC HEADLESS Debug/Release 验证；不声明真实 GPU、原生窗口、Linux、系统 IME/AT 已通过。
