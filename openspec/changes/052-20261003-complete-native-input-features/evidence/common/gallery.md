# 单行家族 Gallery 集成验收

2026-10-03，Windows 11 / MSVC / Ninja Multi-Config。

windows-msvc-headless 完整 CTest：Debug 85/85（101.87 s），Release 85/85（22.43 s）。Core、组件、逻辑场景与 allocation/边界合同通过。

windows-msvc 的 token_gallery_frame：Debug 1/1（25.29 s），Release 1/1（6.76 s）。该测试使用 Recording GPU API 验证平台通用 Gallery 合同，不代表真实 GPU 窗口验收。

新增十五个稳定 ID：四种 Input 变体、grapheme 超限、自定义 byte 统计/去空格 formatter、Email/ref、vector clear、Hover/vector Password、受控显隐开关、Dark Compact Filled Search、Small Underlined Search，以及三个引用/开关操作。实测 133 stable IDs、152 live samples、61 Theme content runs、27 mounted Input/Password/Search/Typography editors；参考目录保持 73 entries、126 reference surfaces/content runs。

四变体顺序、grapheme count=4 / Error、byte count=5 和两个 Search 变体均由 frame 合同断言。宽窄 reflow、Theme/brand/status、输入/IME/剪贴板/Search 与 idle 更新不重新执行 Content，不替换 component/editor/scene owner。完整文档的未滚动测试 viewport 由 30,000 增至 40,000 logical height，以容纳新增样本；真实窗口仍使用实际可滚动 viewport。

README、中英文入口、docs/input.md 和 support-overlay 更新单行原生范围。Input 家族仍为 partial：TextArea、OTP 和 visual bidi caret navigation 未实现；Web 专用 API 明确不移植。

clang-format 22.1.3 check：433 自有源文件，0 failures。OpenSpec doctor healthy、strict 52/52、git diff --check 通过。
