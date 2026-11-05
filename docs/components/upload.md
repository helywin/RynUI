# Upload

`<ryn/upload.hpp>` 提供原生按钮/拖放区域、稳定 UID 文件列表、真实 Image 缩略图及应用传输边界。组件不执行 HTTP，也不制造成功结果。

```cpp
#include <ryn/upload.hpp>

void attachments(ryn::UploadTransport send) {
    ryn::Upload(ryn::UploadProps{}
                    .accept(u8"image/png,image/jpeg,.bmp")
                    .multiple()
                    .maxCount(3)
                    .customRequest(std::move(send)));
}
```

示例由 `rynui_upload_public_api_compile` 编译。应用的 `customRequest` 接收实际 `UploadFile` 与 progress/success/failure completion，返回取消函数。completion 必须在 Runtime owner thread 调用；重复、旧 generation、外线程、删除后或取消后的结果返回 false。progress 为单调有限的 0–100；无 transport 时文件保留 Ready，应用可配置请求并提供显式重试。不存在默认服务器地址、自动网络请求或虚构响应。

`fileList` 完全受控；`onChange` 返回建议列表，应用决定是否接受。UID 必须唯一且稳定；重排/进度更新保留该文件的原生子树。`beforeUpload` 和 `transformFile` 可异步完成，顺序与选择顺序一致；Keep 保留手动上传项，Ignore 不加入列表。`maxCount(1)` 替换现有项，更大的上限截断新项。`onRemove` 可异步拒绝/允许，实际删除后取消请求；受控拒绝删除保留原项。

文件由公开 `NativeFileServices` 提供，Core 不解释路径或读文件。宿主把服务与 window ID 绑定到窗口服务；SDL SDK adapter 把 chooser 回调复制到 owner-thread 队列，按选中的工作区/应用授权路径建立不可变共享快照，图片读取上限单文件 16 MiB、总计 64 MiB、4096 项。目录不跟随递归 symlink；选中的非图片保留 locator/metadata，由应用传输读取。Drop 的临时 SDL 数据在事件内复制，不由组件释放。file paste 使用宿主文件剪贴板，文本编辑器保有自己的粘贴优先级。

隐藏、禁用、readOnly、零尺寸、外部输入边界阻挡或窗口失活停止选择和待发工作，卸载拒绝晚到回调。SDL chooser 无可用的 OS 关闭句柄，因此逻辑取消只拒绝结果；宿主窗口/服务退休后晚到 SDK 回调安全释放。真实 OS chooser/portal 尚未验收，SDK mock 不代表 OS 服务通过。

Text/Picture/PictureCard/PictureCircle 使用同一保留文件模型，后者通过 Image 的真实圆形 shader 裁剪。默认 preview 打开原生 PreviewGroup；download 必须由应用提供回调；remove/retry/abort 为真实模型操作。typed item/icon renderers、trigger/action-icon slots 和 locale 可自定义内容；普通更新保留草稿，显式 renderer 替换局限于该文件内容。用户 callback/predicate/renderer 抛异常时清理失败 owner，取消未完成任务。

`ThemeConfig.upload.tokens` 控制组件视觉，`LayoutStyle` 控制外部布局。`<ryn/form_upload.hpp>` 的 `UploadFormMapping<T>` 明确映射 Form 字段与文件列表；应用保留真正 metadata/locator，并提供 decode/encode 两个方向。adapter 在每次字段回调后重新 decode，UID 值不变的进度也能刷新；额外异步 metadata 可保存在应用 Signal 中。`FormItemProps.control(UploadRef)` 把验证焦点与真实 geometry 连接到原生选择按钮，不把 UploadFile 伪装为文本输入值。Gallery 的传输清楚标注为合成应用服务，进度/成功/失败由按钮显式交付。

API 对照基线为 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 Upload 类型与 demos。originFileObj 映射为 NativeFile；Promise beforeUpload/transformFile/onRemove 映射为可取消单次 completion；customRequest 映射为应用 UploadTransport。action/method/headers/withCredentials 属于应用传输，浏览器 input/DOM 属性不进入 Core。url/response 是应用交付的 opaque 结果。[官方类型](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/upload/interface.ts)。

`UploadLocale.image_preview` 配置共享预览的原生标签，RTL 同步到预览控制器。重排保留当前预览的 UID 与变换；显式 renderer 替换会更新该项的 Image/action 区域，普通进度仍保留草稿。异步 before/transform 的异常会清理对应 Upload owner；外部保存的 item action 在 owner 退休后安全忽略。

Upload 的内部触发与富文件内容不占用外层 FormItem 的自动控件或隐式 Enter；富内容中的显式独立 FormItem 仍保持自己的 typed 绑定及 Enter。reset、disabled/readOnly 和退休继续遵守当前 Form 注册代际合同。
