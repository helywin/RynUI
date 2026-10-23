# Modal Gallery 集成

2026-10-23，实际 Windows / MSVC x64，`windows-msvc` Debug 的 Recording 合同。

- 接入12个保留 Modal：基础、保留编辑、受控拒绝、加载、自定义标题/页脚、无页脚、透明遮罩、居中、长正文、父子嵌套、局部主题；11个页面入口。Gallery 总计212个稳定标识、242个 live samples。
- `token_gallery_frame` 完整通过（142.45s）。新增整页滚动后打开，确认 overlay 不跟随 document translation，编辑/关闭/重开保留草稿和 editor 身份，Select 先消费 Escape，背景滚轮被模态边界消费，父子层关闭及入口恢复，不重跑内容。
- 目录 generator/contract/reference catalog 与 document model/contract/viewport/viewport contract 共7/7（0.18s）。新用例另以 `--modal-only` 独立执行通过，便于定位集成回归。
- support overlay 明确标记 partial，保留 static service、动画、延迟销毁、RTL 等未支持范围；README中英文与常用组件清单同步。
- 格式检查、doctor healthy、strict 61/61、diff check通过。日志在本机 `out/modal-gallery-final2-debug.log`、`modal-gallery-related.log`、`modal-review-format-final.log`。

上述为平台通用行为证据；真实 D3D12 窗口、Linux 与人工系统 IME 不由此勾选。
