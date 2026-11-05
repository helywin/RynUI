# Image 与 PreviewGroup

`<ryn/image.hpp>` 提供真实 RGBA 图像、加载/错误槽和原生预览。源是不可变 `ImagePixels`、`EncodedImage` 或应用 `ImageLoader`；Core 不读取文件或网络。内置解码支持 PNG、JPEG、BMP，其他格式返回 `unsupported_format`；SVG 不属于原生图像合同。尺寸、颜色与方向规则见 [renderer 合同](../renderer-contract.md#image-扩展107)。

```cpp
#include <ryn/image.hpp>

void picture(std::shared_ptr<const ryn::EncodedImage> bytes) {
    ryn::Image(ryn::ImageProps{}
                   .source(ryn::ImageSource{std::move(bytes)})
                   .alt(u8"Inspection photograph")
                   .fit(ryn::ImageFit::Contain)
                   .layout(ryn::LayoutStyle{}.width(ryn::dp(240)).height(ryn::dp(160))));
}
```

本例在 Image 组件测试中编译使用。`LayoutStyle` 控制外部布局；视觉由 `ThemeConfig.image.tokens` 控制。仅指定一个尺寸时保持解码图像的宽高比；Cover/Contain/Fill/None/ScaleDown 明确定义适配。透明像素使用 straight alpha。应用在需要时预先规范化 JPEG EXIF 方向。

`ImageLoader::start` 返回取消函数。应用完成 IO 后必须在 Runtime owner thread 调用 completion，或先通过自己的 dispatcher 投递；返回 false 表示外线程、过期、重复或取消结果被拒绝。隐藏、禁用、窗口失活、输入边界阻挡和卸载停止挂起任务；恢复后以新 generation 加载。失败可显示 typed error slot 或真正解码的 fallback；无默认 HTTP 请求。

Enter/Space 或指针打开预览；Escape 先交给正在编辑的输入，再关闭当前预览。方向键导航/缩放，滚轮缩放，拖动遵守旋转后的边界。工具栏提供旋转、翻转、重置与关闭。`ImagePreviewOptions` 控制比例范围、键盘、拖动、遮罩关闭和 focus trap；关闭/失活释放 capture。预览使用原生 Modal 的嵌套焦点/Tab 清理。关闭 focus trap 只释放本层的限制；仍打开的底层 Modal 继续约束输入，独立兄弟模态层保持顶层可操作且不透传背景。Escape 由最内层 Popover/Modal 仲裁。`actionsRender` 接收保留的 Signal 与 action handle；源或缩放改变不会重建自定义输入草稿。

`PreviewGroup` 接收唯一稳定 key 的 `ImagePreviewItem`，支持受控 `open/current`，请求通过 `onOpenChange/onChange` 返回；应用拒绝请求时保留受控值。重新排序保留同 key 的缩略图与当前变换。`thumbnails(false)` 提供独立预览控制器。`previewSource` 可在打开单张预览时加载更高分辨率源。`alt` 和 locale 用于原生可见标签；尚无系统辅助技术桥接的验收。

API 对照基线为 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 Image/PreviewGroup 类型与 demos。src/fallback/preview.src 映射为 typed ImageSource，placeholder/cover/toolbarRender 映射为 typed 保留槽/renderer；current/open 与转换操作保留其原生语义。HTML 属性、浏览器 URL 抓取、SVG/动画格式不映射为 Core 功能。[官方源](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/image/index.tsx)。

分组同时更新 items/current 时使用最新的一对受控值；Upload 按稳定 UID 保留当前预览与变换。受控 open 在临时空列表期间保留应用意图，图片恢复后重新呈现。Next/Previous 始终表示数据顺序，RTL 镜像方向键和工具栏位置。保留的 action 函数在外线程、旧 generation 或 owner 退休后安全忽略。
