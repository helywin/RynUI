# 4.1 API 与 Gallery 共同验收

2026-10-03，实际 Windows/MSVC，Ninja Multi-Config，BUNDLED。

`windows-msvc-headless` 完整 Debug/Release CTest 各 99/99，249.72s / 28.48s；包括 Unicode bidi 全量 corpus、dependency/Core boundary、Input 家族和 20,000 次零分配更新回归。平台通用 Gallery frame/catalog 在 `windows-msvc` 执行：Debug frame 30.39s、Release frame 8.49s；generator/contract/reference catalog/document model 均通过，Release 同组 5/5，8.87s。初次 C++ catalog 测试发现 Flex/Space 在 049/050 完成后仍保留 partial 断言，已改为核验 implemented 与 implementation/test evidence；Debug 最终 catalog 1/1，0.05s。

公开 standalone headers 验证 TextDirection 及 TextProps/TypographyProps/TitleProps/Input/Password/Search/TextArea fluent typed 返回类型。组件 fixture 验证初始 RTL、reactive LTR、非法方向拒绝及 Auto 恢复，方向变化恰好一次相关 shape，原 editor/ref/scene/selection/session 保留，root 和 prefix slots 均只运行一次。Typography display/editor 透传同一 direction。TextDirection 只规定段落基础方向，OTPDirection 仍控制组排列。

Gallery 增加 6 个稳定 ID，159 IDs、178 live samples、63 Theme scopes、84 input editors；七类 73 reference entries、126 reference surfaces/内容运行不变。新增普通文字、带 underline 的多行 Paragraph、混合 Input、RTL TextArea、Password 与方向切换按钮。目录 Input 标为 native implemented，Web API 排除、Linux native/Wayland 与 OS 候选 UI 证据边界保留。

补 README、正式 architecture、双向文本/Input 文档，保留 Typography/ConfigProvider 其余范围。clang-format 22.1.3：457 owned source files，0 failures；doctor healthy、strict 55/55、diff check 通过。本阶段不以 headless/Gallery fixture 替代真实系统窗口/GPU/字体/输入验收。
