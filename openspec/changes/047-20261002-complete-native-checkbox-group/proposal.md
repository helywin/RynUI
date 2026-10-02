# Proposal

## Why

用户要求补齐已有组件的原生桌面功能。现有 Checkbox 只有单项 checked/indeterminate、标签和基础交互，缺少原生 Group、多选 options、焦点 ref、RTL 和独立组件主题；Gallery 因此仍为 partial。

## What Changes

- 新增 CheckboxGroup、reactive options/value/disabled、默认多选值、稳定顺序的 onChange，以及保留的手工内容与 skipGroup。
- 新增原生 string/finite number/bool 标识值、CheckboxRef/autoFocus/onClick、scoped RTL 与有限 wave；保持旧 Checkbox 调用。
- 补齐 Checkbox 原生主题解析、组件 seed/algorithm、选中/半选/禁用/hover/focus 与标签语义；共享 retained surface 和动画生命周期。
- 更新 Gallery、原生功能覆盖目录、API 文档、共同合同和 Windows 真窗口证据。Linux 原生验收独立记录。

非目标：DOM/CSS/HTML、React synthetic event/ref、Web form/name/required/id/ARIA、浏览器提交行为不移植。Radio 原生收尾另建 change；本 change 验证其旧合同不回退。

## Capabilities

### New Capabilities

- `checkbox-selection`：原生 Checkbox 单项/分组状态、稳定 options、内容、焦点、主题与有限反馈。

### Modified Capabilities

无；openspec list --specs 当前没有主规范。

## Impact

公开 checkbox.hpp/rynui.hpp、selection component、Theme 与 token identity/JSON/goldens、Gallery 生成器和测试、原生 readback 夹具。无需新第三方依赖，不改变 GPU ABI；Core 不依赖 renderer/SDL。

风险：动态 options 的销毁/重排焦点、回调中销毁与受控方拒绝回写、共享 Radio/Switch 回退、主题 identity 漏更新。通过 retained identity、回滚、生命周期、完整 headless 与真实平台输入/缩放验收验证。
