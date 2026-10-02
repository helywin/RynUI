# Icon

`Icon` 使用锁定的 Ant Design 离线资源，共 848 个 typed 名称：447 outlined、251 filled、150 two-tone。`IconName` 使用 `uint16_t`；原先十四个名称的数值保持不变。字体、名称和层角色由同一 manifest 生成，来源与许可见 [第三方依赖](development/third-party.md)。

```cpp
Signal<IconSource> source{IconSource{IconName::HeartTwoTone}};
Signal<float> angle{0};
Icon(IconProps{}.source(source).rotate(angle));
Icon(IconProps{}.name(IconName::LoadingOutlined).spin(true));
Icon(IconProps{}.name(IconName::WalletTwoTone)
         .twoToneColor(IconTwoToneColor{Color::rgba8(22, 119, 255), Color::rgba8(230, 244, 255)}));
```

`name(Prop<IconName>)` 和 `source(Prop<IconSource>)` 是同一来源的两个入口，最后一次 setter 生效。`rotate(Prop<float>)` 是顺时针角度，必须有限；绕图标 em 方框中心旋转，布局占位不变。`visible(false)` 清空全部层并释放布局占位，重新显示时保留组件与 scene 身份。

单色图标使用所在 slot 的语义前景色，或 Text Theme/tone。双色图标默认使用 Theme 的 `color_primary`；显式 `tone` 使用对应语义文字色。`twoToneColor` 覆盖主色，可指定副色；省略副色时使用锁定 Ant Design palette 的第一阶浅色并保留主色 alpha。每层的 source opacity 与祖先内容透明度相乘。

每个 Icon 只有一个 Component/root；内部每个颜色层持有独立保留 TextScene，层的 pivot、位置、字体与 clipping 一致。源变化复用共同层前缀，删除多余层；颜色更新只写 material，旋转只写共同 glyph geometry，均不重新 shaping 或上传 glyph coverage。

`spin(true)` 使用窗口 AnimationRuntime，默认一圈为十个 `motion_unit`（默认一秒）。显式角度与 spin 相加。reduced motion、Theme `motion=false`、`visible(false)`、失活 slot 或窗口失活停止动画并恢复显式角度；恢复有效状态后重新开始。卸载清理所有额外 scene、动画 scope/target，静止图标不保留下一帧 deadline。

本页记录已实现的内置图标入口；typed 自定义向量与 Gallery/真实窗口验收由 change 051 的后续阶段补齐。平台通用测试见 `tests/icon_component_tests.cpp`，窗口/GPU 证据独立记录。
