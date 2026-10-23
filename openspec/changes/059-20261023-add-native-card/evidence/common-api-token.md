# Card API 与 Token 阶段

2026-10-23，实际 Windows MSVC x64，`windows-msvc-headless` Debug/Release。

新增 CardProps、CardSize/CardVariant、CardContent 与 CardTitle/Extra/Cover/Actions typed slots；加入 umbrella header。此阶段提供声明，尚未提供容器挂载实现，Gallery support 保持 planned。

Card Token 含9个颜色、12个尺寸、两尺寸标题字体/正文语义字体与普通/hover 阴影，接入 snapshot、hash、诊断 JSON、继承/reset、独立 component seed/algorithm 和显式 override。新增4个 dirty identity，颜色仅材质、尺寸测量/命中、字体文字/测量、effects 几何/材质；非法尺寸、字重及溢出 extent 不发布部分 snapshot。

默认/小号 Card header 高56/38、padding24/12、border1、radius8；配方来源已在 design 记录。五份 golden 仅增加 Card 与改变整体 identity；`verify_existing_tokens.py` 按值对比规划提交0ef80e0，确认此前所有组件（含 Select）值未变。

```text
Debug build: passed; affected CTest 20/20 (1.93s)
Release build: passed; affected CTest 20/20 (1.53s)
filter: card_|theme_|select_|public_api|backend_boundaries
5 golden value preservation checks: passed
clang-format 22.1.3: 472 owned files, 0 failures
OpenSpec doctor: healthy
OpenSpec strict: 59/59, exit 0
git diff --check: passed
```

真实容器、loading 保留状态、hover、Gallery 与原生平台证据仍在后续阶段，未勾选。
