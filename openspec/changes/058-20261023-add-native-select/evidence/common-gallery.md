# Gallery 与公共集成验收

日期：2026-10-23。实际平台：Windows/MSVC x64，preset `windows-msvc`，Debug/Release。本阶段是平台通用 Gallery/Recording 合同；不作为真实 GPU、系统输入或 Linux 验收。

- Gallery 新增16个真实 Select、3个 ref/open/options 操作按钮与19个稳定 ID：controlled 搜索/清空、int64、disabled、loading、空列表、controlled open、四变体、三尺寸、warning/error、局部 Component Token 与装饰 prefix。旧 Input/OTP/bidi 样例顺序保持。
- Select 元数据从 planned 改为 partial，明确 multiple/tags、分组、自定义行、虚拟化、内置异步数据源与交互式附件未实现；catalog、README 和常用组件清单同步。
- 修复 Gallery 页面滚动给窗口层再次平移的问题：遍历在 `clip_boundary` 停止，窗口层使用自己的窗口坐标，内部菜单裁剪保留。单元验证边界和其子节点不被滚动平移；完整 Gallery 验证滚动后菜单跟随选择器，点击候选项穿过真实 controlled binding，组件/行 identity 与根 content runs 保持。
- 长跳滚动继续逐记录禁止实际字形几何重建。空 Input 文本记录没有字形，其空范围 placement 更新不再误算为已有字形重建；没有改动 TextSceneService。

验证：

```text
windows-msvc Debug/Release builds: passed
Debug Gallery contracts: 7/7 passed; final token_gallery_frame: 1/1 passed (58.79s)
Release Gallery contracts: 8/8 passed (12.74s)
Debug Select/Input/SDL/platform affected tests: 11/11 passed (4.28s)
catalog generator --check: passed
clang-format 22.1.3: 468 owned files, 0 failures
openspec doctor: healthy
openspec validate --all --strict --no-interactive: 58/58 passed
git diff --check: passed
```

完整 common headless 103/103 Debug/Release 已在交互阶段完成，见 `common-interactions.md`；此阶段没有修改 Core。Windows 实际窗口矩阵与 Linux native checkbox 分别保留。
