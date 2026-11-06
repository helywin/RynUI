# Watermark

原生合同：`Watermark(WatermarkProps, WatermarkContent)`。Windows/MSVC Debug/Release 定向测试和真实 D3D12 Gallery 验证已完成，结果见[验收摘要](../../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/decoration-windows.md)。

`content` 支持 UTF-8 文字与换行；`image` 接受共享 `ImageSource`（不可变像素、编码数据或宿主异步 loader）。显式图像来源优先，失败通过 `onError` 报告，不把失败图像偷偷换成文字。清空图像来源恢复文字。`onLoad` 每次有效请求成功后触发一次。

`size` 声明未旋转水印的逻辑尺寸，`gap` 是重复单元之间的间隔，`offset` 是相对内容左上角的起始偏移；省略 offset 使用半个 gap。`rotate` 使用角度，支持响应更新。旋转由现有 `GlyphTransform` / `ImageInstance` 消费；文本共享 shaping 状态和独立 retained views，图像共享不可变像素，均裁剪到实际内容与祖先 clip。可见区域超过 4096 个重复单元时明确抛出 `length_error`，不截断或静默丢水印。

文字颜色、字号、行高、字体族与图像透明度通过 `ThemeConfig.watermark.tokens` 控制。内容尺寸、主题、间距、偏移和旋转更新保持真实子内容身份；静态水印不申请动画 deadline。

loader 必须在 Runtime owner thread 完成；外线程、重复、取消和失效组件的 completion 返回 false。切换来源、隐藏、禁用和销毁先使旧 ticket 失效，再调用宿主 cancel；取消后恢复可重新请求。解码、字体、图像上传及设备能力沿用共同资源合同，不提供组件私有网络或上传路径。Gallery 有文字/图像切换、旋转、间距、尺寸、主题和异步成功/失败操作。

自动原生检查使用 hidden readback，原始输出仅放 `out/`。已验证 Windows 系统字体、1.5× scale 与四组宽度/主题；Linux、其他实际 DPI、系统 IME 和 AT 不在本次证据范围。
