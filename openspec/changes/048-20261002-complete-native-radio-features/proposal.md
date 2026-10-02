# Proposal

## Why

Radio 已有圆形单选和静态字符串 Group，但尚未覆盖现有组件的完整原生桌面合同。用户要求收尾已有 partial 组件，本 change 补齐动态组合、按钮形态、键盘导航和主题，而不移植 Web 专用 API。

## What Changes

- 保留现有 String value/回调入口，增加 String/double/bool 类型值入口、reactive options 和 typed RadioGroupContent。
- 最近 Group 管理手工 Radio/RadioButton；动态更新保留匹配值的身份，清理删除项的焦点、capture、动画与资源。
- 增加 RadioButton、outline/solid、三种尺寸、block、横纵组合、RTL 和相邻边界；视觉由 Theme 控制。
- 增加 RadioRef、autoFocus/onClick、单一组 Tab 入口、方向键循环选择和有限波纹。
- 增加独立 Radio token 覆盖、Gallery 样例与真实 Windows 验收；Linux 项保持独立。

## Capabilities

### New Capabilities

- `radio-controls`: 原生单选、动态组、按钮形态、键盘焦点、主题与资源生命周期合同。

### Modified Capabilities

无。当前主 spec 库为空，既有 change 不自动归档。

## Impact

涉及公开 radio.hpp、SelectionComponentHost、FocusManager 的 Tab 资格、共同表面边界表达、Theme resolver/runtime、Gallery 和测试。保持 logical CPU scene 与共同 renderer ABI 边界；不引入 SDL 到 Core，不增加第三方依赖。DOM/form/name/nativeElement、CSS/React 事件与兼容别名不移植。
