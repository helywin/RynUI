# Design

## Context

见 proposal.md。现有 SelectionComponentHost 为三种选择控件共同保留 scene，RadioGroup 仅静态 String options；047 已建立动态 CheckboxGroup 和有限反馈生命周期。锁定参考为 Ant Design 6.6.5 [interface](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/radio/interface.ts)、[group](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/radio/group.tsx)、[style](https://raw.githubusercontent.com/ant-design/ant-design/6.6.5/components/radio/style/index.ts)，检查日期 2026-10-02。

## Goals / Non-Goals

Goals：保持已有 String 用户源码兼容，统一类型值与动态内容；加入可实际操作的按钮单选、键盘组导航和独立主题。Native Core 与共同 renderer 边界不变。

Non-Goals：DOM/nativeElement、form/name/required、React 事件与 CSS 入口；不借本 change 归档历史任务，不把单平台结果描述为跨平台验收。

## Decisions

1. `RadioValue=variant<String,double,bool>`、`RadioSelection=optional<RadioValue>`；既有 `.value(Prop<optional<String>>)` 与 `.onChange(String)` 保留，新增 `.selection(Prop<RadioSelection>)` 与 `.onValueChange(RadioValue)` 消除重载歧义。两种受控入口互斥；成员 value/defaultValue 接受 RadioValue。String 回调仅对应 String，类型回调覆盖三类。
2. Group options 为 Prop<vector<RadioOption>>；typed RadioGroupContent 采用最近祖先匹配，options/content 互斥，嵌套组独立。借用 047 的有限值验证、稳定生成项、事务式 append、按值保留与删除清理。上限 1024 防止无界二次匹配。
3. Interaction 单独记录 Tab 资格，不改变 pointer/programmatic focus 资格。组选中可用项为入口，无可用选中时以首项为入口；方向键使用复制目标与候选，deferred focus 避免嵌套 dispatch，Space 不允许取消选择。
4. RadioButton 保持选择逻辑并使用共同 logical 表面；CPU 形状表达外侧角与直边，组件不修改 packed GPU ABI、不分支 backend。组合布局管理逻辑顺序、共用边界、等宽 block、H/V/RTL，富标签只挂一次。
5. 新增 Radio Component Token：上游公开 token 与内部 radioColor/radioBgColor 适配，原生 focus/wave 和字体/几何补充字段明确记录。按 colors/metrics/effects 身份订阅，保留 resolver/hash/JSON 和旧 token 值；新 golden 只增加 Radio 与身份段。
6. 验证采用 MSVC Ninja Multi-Config：平台通用 `windows-msvc-headless` Debug/Release，原生 `windows-msvc` Debug/Release；真实 D3D12 窗口系统和 1/1.25/1.5/2 缩放、readback、idle/deadline 独立记录。Linux 仅在实际机器验证。

## Risks / Trade-offs

- [动态 Group 回调销毁] → 先复制回调与候选，发布后只重新查找存活身份。
- [旧 String API 迁移] → 保留原签名，新增独立类型入口并测试旧用法。
- [按钮相邻角与焦点形状] → 使用共同形状与独立几何测试、真实截图，保持 renderer ABI 守卫。
- [主题扩大栈压力] → 复用大 Prop 的共享不可变存储，Gallery 分小函数；Debug/Release 真窗口烟测覆盖默认栈。
- [动态更新顺序失败] → 验证发生在发布前，append 失败回滚旧 options；限制选项规模。
