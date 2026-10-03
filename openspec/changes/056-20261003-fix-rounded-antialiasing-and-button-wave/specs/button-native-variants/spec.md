## Purpose

使原生桌面 Button 的默认点击反馈符合锁定 Ant Design 的轮廓、色带、扩展与淡出时序，同时保持 typed Theme 参数、原生激活入口、retained 身份和有限生命周期。

## ADDED Requirements

### Requirement: Default button wave follows Ant Design reference

可操作有边框 Button 的默认 wave SHALL 沿按钮真实圆角轮廓向外产生无 blur 色带，400ms 从0扩展到6 logical px、2000ms 从0.2淡到0，两者独立使用 motionEaseOutCirc。颜色 MUST 优先可见 border，其次非白色/非透明 background，再 theme primary；不得出现默认游离细环。

#### Scenario: Spread completes before fade
- **WHEN** Button 激活后已过去400ms但未到2000ms
- **THEN** 外扩达到6 logical px而 opacity 仍大于0，色带内边缘贴合按钮轮廓，不改变尺寸、命中或焦点

#### Scenario: Theme override
- **WHEN** 应用覆盖 wave_spread、wave_width 或 wave_opacity
- **THEN** 参数控制最终外扩、随扩展增长的最大带宽与初始透明度，width受spread限制，0宽度或0扩展无波纹，公开方法仍有效

### Requirement: Independent wave channels retain finite lifecycle

扩展结束 SHALL 保留淡出通道，淡出结束移除 wave；wave=false、disabled/loading、Text/Link、motion off/reduced、window inactive 或销毁 MUST 取消两个通道与未来请求。重复 activation 重启同一个 retained wave，不重新执行内容、测量或增加活跃效果。

#### Scenario: End and repeated activation
- **WHEN** 用户在淡出阶段再次激活，然后等待最终2000ms截止
- **THEN** 扩展/淡出一起重启而不积累效果，截止后无 wave effect/deadline，Content只挂载一次
