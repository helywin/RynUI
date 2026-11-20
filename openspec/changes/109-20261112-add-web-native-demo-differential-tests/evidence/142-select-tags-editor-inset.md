# Tags 空编辑器内边距

- Windows MSVC HEADLESS Debug `rynui.portable.select_component` 1/1 通过。覆盖空 Tags 首个编辑器、已选标签紧凑间距、清空、RTL/小尺寸、富前缀与编辑器身份保留。
- Windows MSVC Release 原生 Gallery 构建通过。后台 D3D12 回读完成 Select 的十个检查点。在 1.5× 初始 Tags 场景中，首字形起点由 4dp 修正为 12dp，固定 Chromium 参考也为 12dp。原来该行物理坐标 2336/2337 处相差至少 4 个 RGB 代码的像素分别为 86/107，复测两行均为 0。
- 完整 paired pilot 与 Linux 此变更的原生验证仍待执行。
