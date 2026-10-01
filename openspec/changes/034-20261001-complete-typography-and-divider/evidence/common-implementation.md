# 平台通用实施证据

实际环境：Windows，`windows-msvc` / Ninja Multi-Config / MSVC x64，Debug。

## 4.3 内容范围与装饰

复核已有提交发现内容 range 与 surface ID 混用、数量变化不重映射后续 range、缺少内容范围销毁、同数量更新全量重建等缺陷。修复后，以混合 surface/content 的增长、收缩、材质更新、等值更新、销毁和 generation 重用用例验证共享实例与片段一致。

复核 Typography 发现 underline/strikethrough 未绑定、挂载末尾覆盖 code/keyboard 的实际字号、disabled/secondary 优先级及 disabled 组件 Token 订阅缺口、标题字重和间距未实际生效。均已修复并加入行为断言。

装饰片段常驻，允许挂载后从 false 改为 true。背景与圆角边框先于字形，字体度量装饰线后于字形。圆角边框使用共享 RoundedEffectStore 的 outline，避免半透明边框染色填充区域。颜色与 inline 度量采用独立 TokenIdentity。

`rynui.typography_component` 验证命令顺序、实际字体尺寸、逐 run em 度量、十行二十条装饰线、重排缩减、身份保持、滚动 translation、裁剪、纯颜色更新只修改材质，以及 display scale 1.0/1.25/1.5/2.0 的逻辑几何与标题间距。

受影响回归：`theme_runtime`、`button_scene_service`、`input_pointer`、`input_keyboard`、`input_scene_allocation`、`button_component`、`text_component`、`typography_component`、`text_component_frame` 通过。Input 交互与场景分配合同保持通过。本节为 headless/contract 证据，不代表真实 GPU 窗口验收。
