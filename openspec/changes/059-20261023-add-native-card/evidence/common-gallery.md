# Card Gallery 集成

2026-10-23，实际 Windows/MSVC x64，`windows-msvc` Debug/Release。这里验收平台通用 Gallery/Recording 合同，不代替实际 GPU 或 Linux 窗口结果。

新增7个真实 Card：无标题、标题/extra/Input 与可切换 loading、富标题/cover/actions、小号、borderless/hover/子 Button、静态 loading、局部 Component Token；另有1个独立 loading 切换按钮。新增8个稳定 ID，原有 Input/Select 索引保持不变。

支持目录改为 partial 并列出首版缺口：Grid/Meta、内置 Tabs、异步图片、骨架动画、任意后代圆角遮罩与 Web API。catalog 由生成器同步，README/常用组件清单同步。

完整 Gallery Recording fixture 验证实际 Card Input 编辑、点击对应 loading 按钮、隐藏/恢复正文及输入会话，确认草稿内容、editor/body identity、组件总数与根/Theme content runs 保持。继续通过宽窄视口、颜色/紧凑/品牌切换、整体滚动、Select 窗口层、旧编辑控件与 idle 合同。

```text
windows-msvc Debug/Release builds: passed
Debug affected Gallery CTest: 8/8 passed (62.40s)
Release affected Gallery CTest: 8/8 passed (13.55s)
catalog generator --check: passed
clang-format 22.1.3: 475 owned files, 0 failures
doctor: healthy; strict: 59/59, exit 0
git diff --check: passed
```

平台通用完整106/106 Debug/Release 见 `common-container.md`。Windows 实际窗口与 Linux native 继续独立记录，不自动勾选。
