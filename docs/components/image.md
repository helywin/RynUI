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

107 通过语义吸收 `0cd4817b` 的 RoundedRectangle 与预览懒挂载接口（源基线 `135ba70b`），保留当前 paint opacity 合同。`ImageShape::RoundedRectangle` 使用 Image Theme 的 radius，Rectangle/Circle 的序号不变；不提供公开 radius/style setter。圆角图像由共同 logical 几何分片与 UV 子范围表达，片段共享 renderer/common 的不可变像素纹理 lease；同一资源域中只分配/上传一次源纹理，片段退休不使其他 lease 失效。Image ABI v2/112 bytes 与 shader 不变。

`preview(false)` 初始不创建 Modal、工具栏或焦点边界；响应更新 enabled 后惰性挂载并复用。禁用会关闭预览和清理 capture。Card 封面可向直接 Image 子组件传内部上角策略，来自 Card token；显式 Circle/RoundedRectangle shape 优先。自有背景、hover 和边框遵循同一上角策略；它不裁剪 slot 中的任意后代。这个策略不进入预览，不传播到任意后代，替换区域后不泄漏到别的 owner。

预览 actionsRender 在私有 Form 编辑作用域内执行，工具栏内部 Input 不认领外层 FormItem 的自动 value/Enter 绑定；调用者显式声明的嵌套 FormItem 仍使用自己的绑定。

108 媒体接入：未调用 `locale(...)` 时，Image 与 PreviewGroup 的内置加载、错误、预览操作和计数继承最近 ConfigProvider 的 image 语言组；显式完整 ImageLocale（包括英文和空字段）优先，typed slots 保持原有优先级。未设置的 `alt` 可使用本地化错误文案，显式 `.alt(u8"")` 保留空内容；ImagePreviewItem 的旧 aggregate 空 alt 仍视为未提供，不在内部转发时封死默认文案。业务 alt、分组 key 与图片内容不翻译。`ImageLocale.count` 使用 `{current}`、`{total}` 无符号整数模板，显式空模板隐藏计数。

语言更新只重投影 retained 文案，不重新加载 source/previewSource、不改变受控 open/current 或预览变换，也不重跑 actions renderer。新增 `rynui.i18n_components_media` 覆盖真实组件行为；本子阶段交接尚未编译、运行或进行 native/GPU 验证，验收状态由父线程实际结果更新。
