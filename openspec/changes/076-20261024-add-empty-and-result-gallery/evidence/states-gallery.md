# Empty/Result Gallery 验收（2026-10-25）

## 范围与环境

在 main `0bc5c97b99520549576b2d9028c943f2450d69a2` 上恢复 076 草稿。实际环境为 Windows、MSVC、CMake `windows-msvc` preset、Ninja Multi-Config、Debug。以下 Recording 检查使用真实生产组件与 logical scene，不是平台 GPU/系统 IME 证据。

追加 3 个 Empty、3 个 Result：创建操作/响应式说明、Default/None 图示切换、Compact、自定义带 margin 图示、七状态循环、继续操作、保留 Input 草稿、静态 404 与自定义 Flex 图示。保留已有样例的顺序和身份。

## 平台通用：聚焦场景与合同

构建命令：`out/typography-dev.ps1 -Preset windows-msvc -Target @('rynui_token_gallery','rynui_token_gallery_frame_tests')`。已构建的 `rynui_token_gallery_frame_tests --states-only` **退出 0**，纳入完整 main，实际验证：

- 通过平移后的 1200×900、560×900 视口，发送真实 pointer、Space 和文本输入事件；四个新 Button 均激活实际回调。
- 七个 Result 状态的真实 Icon source、glyph 与标题更新；Empty 隐藏/恢复图示时实际 flow 与 scene 恢复。
- Input 草稿和 Component/editor/interaction/Text scene、按钮/slot 身份跨状态、图示、Default/Dark/Compact 和宽度更新保持。
- 自定义 Icon 的 100 dp 内容宽度、左右各 50 dp margin 及外框居中；所有新增组件区域的实际可见 glyph 受有限 clip 约束。

`rynui_ant_design_reference_catalog_tests` 重新构建后运行 generator、catalog contract、reference catalog、document contract、viewport contract、token Gallery contract、evidence schema、Windows acceptance runner contract，**8/8 通过，0.17 秒**。格式检查 **583** 个自有源文件、doctor、strict **76/76**、diff check 通过。ignored 日志：`out/states-gallery-{build,final-build,focus-rebuild,theme-rebuild,focused-pass,contracts}.log`。

实际 `--inventory-only`：**273 IDs、93 navigation、308 Buttons、906 interactions、583 visible interactions、279 live、67 Theme content runs、135 Inputs、21 Select、13 Modal/Drawer、45 selection、2 Menu、1 Dropdown、1 Collapse、3 Segmented、3 Empty、3 Result**。Table 在先前 wheel 修复中新增的不可聚焦 body 背景 interaction 已计入；不能只按此次四个 Button 和一个 Input 推导旧基线。

初跑发现并修正三个测试预期问题：普通反馈 Text 的 ink 比 advance 右边界外伸 1 px，应检查其实际父 lane clip；FocusManager 重复请求已聚焦目标返回“未变化”，应验证最终 focused；Compact Theme 会改变 compact_image_size，实际节点需符合当前 component token，不能在所有主题硬编码 40。保留严格的 Empty/Result 区域 clip、真实 glyph 与身份断言。诊断日志为 ignored `out/states-gallery-{focused,diagnostic,focused-final,focused-verified}.log`，最终聚焦重新执行通过。

## 集成验收

完整 Gallery frame 与八项合同尚待最终运行收口，不提前勾选 2.1。

Windows 原生 `rynui_token_gallery --smoke` 已实际执行：**D3D12/DXIL，display scale 1.5，273 IDs、279 live，7 frames，exit 0**。日志为 ignored `out/states-gallery-native-smoke.log`。这只证明实际窗口启动、自动主题切换与退出；不代表新增组件逐项 GPU/人工视觉、系统 IME、Linux 或屏幕阅读器验收。

## Astra R1：实际目录入口

`15c356c` 的完整 Gallery **9/9，226.90 秒**（frame 226.37 秒）随后通过，但 Astra R1 发现目录仍定位参考卡片，原聚焦测试通过手工平移进入样例，遗漏 spec 的实际目录流程。先恢复 1.1 未完成，再补齐生产导航；不把原成功结果作为新入口的证据。

Empty/Result 保留原目录按钮和 identity，组件锚点改为实际交互样例的稳定标题；其他组件、分类及章节保留原参考目标。运行时与测试共用组件锚点解析和跳转。聚焦测试实际点击可见目录 Button，消费其生产导航请求，到达样例后操作图示/状态按钮及真实 Input，并检查草稿、身份和可见 glyph；覆盖 Default/Dark/Compact × 1200/560 宽。另通过原 Button 目录验证既有参考卡片定位。

基于已集成 `6f1b38a` 的源代码重新构建 Gallery、frame、reference catalog 及 document model/viewport targets；`--states-only` **退出 0**，document model/viewport 加原八项 Gallery 合同 **10/10，0.52 秒**。clang-format 22.1.3 检查 **583 文件零失败**，doctor、strict **76/76** 和 diff check 通过。ignored 输出：`out/states-gallery-navigation-{build,model-build,focused,contracts}.log`。本修复不增加控件或稳定标识，inventory 计数保持不变；本次最终完整 Gallery、原生 smoke 与 Astra R2 仍待收口。
