# 平台通用：Input 像素对齐与 GPU 资源合同

任务 6.4；Windows MSVC `windows-msvc-debug` 构建成功，相关 11 项 CTest 全部通过。阶段 6.3 提交 `2511dfa` 的全量回归为 193/193 通过；本阶段新增 `rynui.input_gpu`，当前共 194 项测试，未把先前全量结果当作本阶段全量结果。

## 四档比例

- `input_component_tests.cpp` 在 1、1.25、1.5、2 倍实际 `FontRasterConfig` 下运行长中文 composition/commit/Home。
- Input 内部 display scale 与窗口/font resolver 配套。scroll translation 按完整物理像素量化，保留字形缓存的 raster phase；clip 向内对齐，caret/underline 厚度按至少一个完整物理像素量化。
- 检查 End caret 全宽保留、下划线厚度、Home 清除滚动、可见首个 CJK glyph 的 ink bounds 未被裁切；未将 atlas padding 当作字形墨迹。

## GPU 接口与重试

`input_gpu_tests.cpp` 使用真实 Input/FontRuntime/scene、正式 Quad/Glyph/RoundedEffect GPU resource 类和 recording GPU/draw 接口，在四档比例下检查：

- 所有 layer 进入对应 GPU buffer；提交顺序严格匹配 composer。
- 圆角效果 GPU packing 后的 CPU shader coverage reference 保留 active shadow/focus 外扩范围。
- selection range 扩展只上传对应 selected glyph range 与变化的 Quad，不上传无关 effect 或 atlas。
- deferred frame 保留请求，不 draw；重试成功且不重复已经成功的上传。
- 注入 glyph buffer 上传失败后，dirty range 保留，恢复上传能够完成绘制。
- 现有 glyph/quad/rounded-effect shader contract 全部通过；未新增 Input 私有 shader。

这些是平台通用几何、资源与接口级合同，不表示实际 GPU 画面或 native Windows/Linux 输入法已验收。正式 Input 状态 Token、动画及 blink policy 仍属于阶段 7。
