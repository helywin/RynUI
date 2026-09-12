# 平台通用：Input retained scene

## 实现与范围

- 任务 6.3：每个 Input 固定保留 4 个圆角效果身份（active shadow、border、fill、focus）、3 个 Quad 实例（selection background、composition underline、caret）、3 个共享 shaping 的文字层（base、selected、placeholder）。inactive layer 使用 opacity 隐藏；shadow/focus 当前保留但隐藏，其正式 Token 和状态规则属于阶段 7。
- 绘制顺序为 container effects、prefix、editable selection/background 与文字、suffix、underline/caret。显示层 fragment 在所有 owner 完成 glyph range remap 后统一绑定，避免前一个 Input 内容增长后遗留旧索引。
- 容器填充复用现有 RoundedEffect 的零 blur/spread 填充能力，以保留原始圆角形状并消费 ancestor clip；未新增 Input 私有 shader。矩形 selection/underline/caret 使用裁剪后 Quad，glyph 使用各自 clip。
- root 注册 Interaction；布局变化刷新全部已注册交互（包括 Input），不是只刷新 Button。
- 当前仍使用现有通用 Theme 颜色与尺寸。完整 InputTokenSet、交互动画、物理像素对齐、GPU 上传及 native 视觉验收不在本记录中宣称完成。

## 已执行验证

Windows MSVC，`windows-msvc-debug` 构建成功；7 项相关 CTest 全部通过：component scene traversal/composer、Input、Button、Text scene service、Text component/frame。

`input_component_tests.cpp` 的 CPU scene reference 检查：

- 固定实例数、共享 TextState、container/selection/glyph/overlay 顺序；Input hit test 已进入 composer。
- 100 次 selection 更新保持层身份、instance 数、composer topology、glyph scene topology 与 shape count 不变。
- Theme border/background 改色只改材质；隐藏 shadow/focus 保持 packed identity。
- 窄 ancestor clip 不改变圆角容器原始 bounds，hit clip 与之匹配；placeholder 切换不销毁 layer。
- 前一个 Input 文字增长、销毁后，后续 Input 和普通 Text 的 glyph/quad/effect 范围仍有效且没有丢失或重复，剩余 Input hit bounds 正确重定位。
- mount-time destroy/reuse、抛异常 slot 回滚、正常销毁均清理文字、Quad、effect 与交互资源。

此处是平台通用 CPU 证据，不是 native 输入法、截图或 GPU 画面验收，也不是 6.5 的零分配 benchmark。
