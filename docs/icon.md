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

自定义图标使用不可变 `IconVector`，同样通过 `source` 绑定：

```cpp
IconVector diamond{
    IconViewBox{0, 0, 100, 100},
    {IconPath{IconColorRole::Primary,
              {IconMove{{50, 0}}, IconLine{{100, 50}}, IconLine{{50, 100}},
               IconLine{{0, 50}}, IconClose{}}}}};
Icon(IconProps{}.source(IconSource{diamond}));
```

命令坐标是 viewBox 内的绝对坐标，y 向下。支持 `IconQuadratic{control, to}` 和 `IconCubic{control1, control2, to}`；每个 contour 必须以 Move 开始、包含绘制段并以 Close 结束。填充使用 nonzero winding，孔洞由反向 contour 表达。非正方形 viewBox 保持比例并在 em 方框中居中。每个 path 的 primary/secondary 角色与 0–1 opacity 按原顺序绘制。

构造时完整校验：1–64 paths、总计至多 4096 commands、有限正 viewBox、有限可表示坐标、合法轮廓/角色/透明度；生成字体至多 2 MiB。极端坐标超出 Type2 16.16 范围时抛出 `std::invalid_argument`。向量在构造后只读，复制共享来源身份；相同来源/字号/density 共享窗口字体缓存。内置/自定义 source 切换仍保留 Icon Component/root 与共同 layer 前缀；窗口 TextSceneService 销毁时释放缓存的字体 bytes、FreeType faces 和 shaping 资源。

内存字体依据 [Adobe CFF](https://adobe-type-tools.github.io/font-tech-notes/pdfs/5176.CFF.pdf)、[Type2](https://adobe-type-tools.github.io/font-tech-notes/pdfs/5177.Type2.pdf) 和 [OpenType 字体表合同](https://learn.microsoft.com/en-us/typography/opentype/spec/otff) 构造，Quadratic 转换为 Cubic；运行时不依赖外部生成工具。平台通用测试见 `tests/icon_component_tests.cpp` 和 `tests/icon_vector_tests.cpp`；Gallery/真实窗口验收由 change 051 后续阶段独立记录。

2026-10-03 平台通用完整 HEADLESS Debug/Release 75/75、Windows 受影响原生 17/17 和默认 Gallery smoke 已通过；十次 D3D12/DXIL 真窗口与 180 GPU readback 见 [Windows 证据](../openspec/changes/051-20261003-complete-native-icon-catalog/evidence/windows/README.md)。Linux 原生窗口验收仍为独立待办，见 [任务清单](../openspec/changes/051-20261003-complete-native-icon-catalog/tasks.md)。
