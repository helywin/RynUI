# 真窗口反馈修复：Compact 自动最小尺寸

首次 Windows 真窗口/GPU 读回发现 Compact 把 Button 压成多行并越出控件；此前测试验证了共有边和 editor 身份，未断言自动标签最小尺寸。这是实际功能缺口，已修复并加入回归合同。

Compact 直接 Button、Addon 和 RadioButton/RadioGroup 采用内容 intrinsic 自动最小主轴尺寸；显式 min_width/min_height 覆盖，Input/Search 保留可收缩空间。共享 Flex 分配算法保持不变；嵌套与透明包装器仍使用自己的布局规则。自动最小值只在共同组件测量阶段读取，不重建内容或 scene。

参考 [CSS Flexbox 自动最小尺寸](https://www.w3.org/TR/css-flexbox-1/#min-size-auto) 的内容约束含义，映射为原生 typed 布局行为，不引入 CSS parser。

2026-10-03，Windows `windows-msvc-headless`：受影响 space_compact、radio_features、input_component、button_component，Debug **4/4**（4.82 秒）、Release **4/4**（3.25 秒）。新增测试比较相同普通 Button 与 Compact Button 的原生宽度，确认输入框分配剩余空间、单一 border overlap，文字高度不会越出 Button。

格式、OpenSpec doctor/strict validation 和 diff check 通过；Windows 最终原生 CTest 与 GPU 证据在修复后重新运行，首次探测不作为最终验收。
