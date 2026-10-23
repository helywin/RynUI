# Tabs Gallery 集成

2026-10-23，实际 Windows/MSVC x64，`windows-msvc` Debug/Release。这里验证平台通用 Gallery/Recording 合同，实际 GPU 窗口另行记录。

新增13个真实 Tabs 样例及1个响应标签/禁用切换按钮：受控/保留 Input 与 Select、禁用项、响应标签、Line/Card、三尺寸、Top/Bottom、centered、8项溢出、空集合、未知 key、独立 Component Token。新增14个稳定 ID，原有 Input/Select/Card 索引保持；总 live_samples=219，稳定 ID=200，Input=104、Select=17、Card=7、Tabs=13。

支持目录改为 partial，明确动态结构/editable/lazy/动画、vertical/RTL、富标签/额外栏、overflow popup、自定义 indicator 和 Web API 缺口。生成目录、两份 README、常用组件清单与 API 文档同步。

完整 Gallery fixture 通过实际整页滚动后编辑首个 Tabs 的 Input、打开嵌套 Select、点击受控标签并切回，确认输入会话/弹层清理、草稿恢复、panel/editor identity、mount runs 与根/Theme content runs 保持。继续通过主题/紧凑/宽窄视口、原有 Card loading 与 Select 窗口层，以及 Recording 上传/idle 合同。

```text
windows-msvc Debug/Release complete builds: passed
Debug affected Gallery CTest: 8/8 passed (72.83s)
Release affected Gallery CTest: 8/8 passed (16.44s)
catalog generator --check: passed
clang-format 22.1.3: 485 owned source files, 0 failures
openspec doctor: healthy; strict: 60/60, exit 0
git diff --check: passed
```

完整平台通用110/110两配置见 `common-navigation.md`。Windows 与 Linux 窗口保持独立任务。
