# TextArea Gallery 与完整共同回归

日期：2026-10-03，Windows/MSVC/Ninja Multi-Config。

新增八个稳定 ID：`gallery.text-area.outlined/filled/borderless/underlined/autosize/resize-nowrap/readonly/disabled`。覆盖四变体、grapheme count/clear、minRows/maxRows、无 wrap 双向 resize/wheel、Dark Compact Small readOnly 与 disabled/Warning。

Gallery 当前库存：141 个稳定 ID、160 个 live samples、62 次 Theme content 初次执行、35 个 Input/Password/Search/TextArea/Typography editor；73 个参考组件、126 个参考 surface 不变。新增 8 个输入交互与 5 个清空交互。既有 sample 索引及 ID 保持稳定。Input 目录仍为 partial，明确 OTP 与 visual bidi 尚待后续 change。

验证：

- `windows-msvc-headless` Debug：完整 87/87，189.42 秒。
- `windows-msvc-headless` Release：完整 87/87，25.64 秒。
- Gallery frame 合同为平台通用 Recording/CPU 测试，本次实际在 `windows-msvc` 中运行：Debug 1/1，35.57 秒；Release 1/1，7.87 秒。确认多行编辑 mode/carets、四变体、整个 Gallery 场景/交互库存、retained upload 和输入身份。
- 新增样本使 MSVC Gallery definition 超过默认 COFF section 上限；只对 `rynui_token_gallery_definition` 的 MSVC 编译启用 `/bigobj`，不修改公开库或其他 toolchain 参数。

日志：`out/053-gallery-full-headless.log`、`out/053-gallery-frame.log`。格式、OpenSpec doctor/strict validate、generated catalog 与 diff 校验通过后提交。此阶段不声明真实窗口/GPU 或 Linux 验收完成。
