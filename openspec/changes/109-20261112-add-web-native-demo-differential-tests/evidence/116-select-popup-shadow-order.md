# Select 弹层的多层阴影顺序

Select Token 仍按 CSS 顺序表达多层阴影。发布到保留绘制列表时按从后到前排列，第一层阴影绘制在最后。[Chromium 141 阴影绘制顺序](https://raw.githubusercontent.com/chromium/chromium/141.0.7390.37/third_party/blink/renderer/core/paint/box_painter_base.cc#L254)

MSVC HEADLESS Release 的 Select 组件、原生输入和原生观察测试 3/3 通过。新增反色阴影回归确认列表第一层位于第二层上方。Windows 1.5× 隐藏 D3D12 完成 Select 十个检查点。四个虚拟和异步弹层检查点的三层实际绘制顺序均与锁定 Chromium 一致。上一阶段的滚动定位也在 Windows 重放中通过，四个弹层检查点的可见阴影保持锚点相对位置。整例视觉验收仍开放。
