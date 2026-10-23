# Tabs 保留面板与导航

2026-10-23，实际 Windows/MSVC x64，`windows-msvc-headless` Debug/Release。这里验证平台通用组件与文本/Recording 合同，不代替 D3D12、系统输入法或 Linux 窗口证据。

已接入窗口拥有的 Tabs host。固定标签、箭头和所有面板一次预建，切换仅改变活动分支；旧面板输入会话、焦点与 Select 窗口层会清理，再次显示保留编辑器和草稿。未显示面板的 autoFocus 不启动输入。普通 label/disabled/尺寸/类型/方位/主题更新不重跑 TabContent。

Line/Card、三尺寸、Top/Bottom、无溢出时 centered、单 Tab 入口、Left/Right 环绕跳过禁用项、Home/End、Enter/Space、受控请求/拒绝回写、未知/禁用显式 key、指针捕获与释放均有实际组件测试。溢出箭头和标签栏滚轮保留手动位置，焦点/选择/resize 显露目标；命中与绘制裁剪到视口，标签单行省略。复用现有延迟焦点机制避免命令过滤器内重入。

滚动使用字体实际 display scale 对齐物理像素，避免分数位移改变 raster phase。1/1.25/1.5/2 字体缩放下检查已有非空字形的 shape/instance rebuild/geometry rebuild 计数均保持；允许平移/clip patch 和首次显露时实现尚未生成的字形。另验证祖先分数平移、resize 移除溢出和零宽度。

测试覆盖真实 Input 编辑、Select 打开/切换清理、面板/编辑器身份、disabled 后焦点清理、回调中卸载、捕获时禁用/卸载、非法 Props/key 的挂载前拒绝、颜色不测量、Token 更新、idle 和资源释放。组件保持无 SDL/renderer 依赖，没有改变逻辑场景或 GPU ABI。

```text
windows-msvc-headless Debug build + full CTest: 110/110 passed (201.58s)
windows-msvc-headless Release build + full CTest: 110/110 passed (10.87s)
clang-format 22.1.3: 485 owned source files, 0 failures
openspec doctor: healthy
openspec validate --all --strict --no-interactive: 60/60, exit 0
git diff --check: passed
```

Gallery 和 Windows/Linux 原生验收继续独立推进；未勾选相应平台任务。
