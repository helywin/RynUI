# QRCode

原生 `QRCode(QRCodeProps)` 把真实内容编码为标准 QR Model 2，采用 MIT 的 Nayuki qrcodegen 1.8.0。公开头为 `<ryn/qrcode.hpp>`，纯 CPU 数据/导出头为 `<ryn/qrcode_data.hpp>`。对照 [Ant Design 6.6.5 QRCode](https://github.com/ant-design/ant-design/tree/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/qr-code) 的类型、样式、demos/tests 与锁定的 rc-qrcode 2.0.0；原生合同不依赖 React 或浏览器。

```cpp
ryn::Signal<ryn::QRCodeValue> value{ryn::QRCodeValue{u8"原生二维码"}};
ryn::Signal<ryn::QRCodeStatus> status{ryn::QRCodeStatus::Expired};
ryn::QRCodeRef code;
ryn::QRCode(ryn::QRCodeProps{}
    .value(value).status(status).errorLevel(ryn::QRCodeErrorLevel::H)
    .boostLevel(false).size(224).marginSize(4).ref(code)
    .onRefresh([status] { status.set(ryn::QRCodeStatus::Active); }));
auto png = code.exportPng(464); // bytes owned by the application; no file IO
```

`value` 接受 `QRCodeValue` 的 String、独立优化的 `vector<String>` 分段或原始 `vector<byte>`。文本使用已校验的 UTF-8 String；含非 ASCII 文本显式写入 UTF-8 ECI 26，内嵌 NUL 不会截断。自动选择 numeric/alphanumeric/byte；不提供 Kanji、Micro QR 或 structured append。输入最多 128 段/7089 个 UTF-8 bytes，binary 最多 2953 bytes；实际容量取决于版本与纠错，超限有明确错误，不能截断。`errorLevel(L/M/Q/H)`、`boostLevel(true)`、`minVersion(1)` 可响应更新。纯模型额外允许最大版本和固定 mask，默认在 1–40 中选择最小版本并评估八个 mask。[编码器合同](https://www.nayuki.io/page/qr-code-generator-library)。

`size(160)` 是 logical 正方形外部尺寸，允许 0 至 16384；分数、极窄及 0 尺寸保持有限几何，0 不绘制模块。`bordered(true)` 使用 QRCode Theme 的 padding/border/radius；`marginSize` 按 module 计，原生默认 **4**，范围 0–64。上游默认 0，原生选择标准建议的 quiet zone；显式 0 可匹配该上游选项，应用负责外围空白。`color`、`bgColor` 覆盖拥有的 `ThemeConfig.qrcode.tokens` 前/背景，其余稳定视觉使用 Theme，LayoutStyle 只控制外部布局。编码数据保持原方向，RTL 排版不镜像模块。

`status` 为 Active、Expired、Loading、Scanned。Expired 的刷新 Button 只有配置 `onRefresh` 时显示；它只发出应用回调，不请求网络、不自行修改状态。`QRCodeRef.refresh()` 同样受状态、disabled、visible、窗口/输入边界约束。`statusRender` 提供 typed `QRCodeStatusContext`：响应式 status/locale/disabled 与安全 refresh action；renderer 指针改变时仅替换本地内容，普通状态变化不重跑工厂。`locale` 提供可替换文案。`onError` 返回 Encoding/Paint/Logo 分类；编码失败不显示伪码，后续合法内容可恢复。

`icon(ImageSource)` 复用共享 ImagePixels、EncodedImage 与可取消 ImageLoader，直接像素/PNG/JPEG/BMP 解码均走现有公共入口。没有文件读取、默认网络获取、私有 decoder/GPU 上传，也不创建预览 Modal。`iconSize` 使用 QR authored size 的 logical 坐标，随实际内区等比缩放并裁到 symbol（与上游 size/CSS 内区缩放一致）；模块在 logo 区域挖空，保留透明像素。异步结果限 owner thread、精确加载代际且只接受一次；隐藏、禁用、窗口 inactive、输入边界抑制及销毁取消未完成加载，拒绝旧结果。`onIconLoad` 仅在真实像素接受后触发。

logo 遮挡、低纠错、低对比度、透明背景、过密/过小模块及缺失 quiet zone 可能破坏可扫描性；纠错比例不是任意区域遮挡的保证，应用应控制 logo 尺寸并对最终内容解码验证。默认颜色跟随文本/透明背景，与上游一致；需要独立分享时推荐高对比度和不透明背景。`Color` 拒绝非法 channel/NaN；相同前背景及全透明前景不能作为有效导出。

`QRCodeRef.data()` 返回当前可用编码模型副本；`exportPng(pixelSize)` 返回确定性 RGBA8 PNG，`exportSvg()` 返回完整 SVG（logo 以内嵌 PNG 保存），纯函数还提供 `rasterize_qrcode`。导出包含编码模型、colors、quiet zone、中心 logo/excavation/alpha，排除容器 border/padding 和状态覆盖。PNG 采用整数 module pitch，多余像素居中；pixelSize 至少容纳所有模块、最多 4096，过小/无效/已退役 ref 会明确失败。SVG 保留 logical 位置，无脚本、外链或服务器状态；文件目的地和 IO 归应用所有。未完成/失败的 logo 按当前画面导出无 logo 模型。导出验证见 change107 的 [Linux 证据](../openspec/changes/107-20261104-complete-navigation-and-remaining-components/evidence/qrcode-linux.md)。

Web `type=canvas|svg` 映射为显式原生导出 API，屏幕统一 logical CPU scene；icon URL/crossOrigin 映射为应用持有的 ImageSource/loader。DOM refs、HTML/canvas attributes、CSS classNames/styles、React nodes 与 portal 不移植。Windows/MSVC/D3D12、物理系统 IME、AT 与真实跨显示器未验证。

108 媒体接入：未设置 `locale(...)` 时，内置 expired/refresh/loading/scanned 文案与 `QRCodeStatusContext.locale` 继承最近 ConfigProvider 的 qrcode 语言组。显式完整 QRCodeLocale 优先，英文和空字段不被默认包替换；status renderer 继续拥有自己的内容。语言更新不重新编码 payload、不重载 logo、不触发 onRefresh，也不重跑 status renderer 或重建其中的编辑器。业务 payload、应用错误和导出数据不翻译。

新增 `rynui.i18n_components_media` 覆盖 retained 文案、编码调用计数、异步 logo、重入/退役和作用域；本子阶段交接为未编译、未运行状态，尚无新的 native/GPU 验收结论。
