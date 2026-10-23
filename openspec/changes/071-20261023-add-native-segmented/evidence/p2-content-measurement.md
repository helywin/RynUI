# P2：slot intrinsic 测量与内容居中

2026-10-24，继续干净的 `t3/native-segmented-component` / `ee9bf220a1c32c4a820e2ea1902040105990dbb1`。本次没有合并更新的 main、访问或修改 root checkout、修改 Gallery/global docs、创建子任务或执行远端操作。

## 先复现

保留 `ee9bf220` 的生产源码，只增加回归断言，使用 `windows-msvc-headless` Debug 构建并运行 component test。场景为水平 block、width=300、两个选项：SearchOutlined + Alpha，以及纯文本 MMMM。原有 `BoxLayout{{}, true}` 把 icon wrapper 填满 126 logical px 的内容区，导致：

```text
content geometry scale=1 width=300 size=1 icon=126 label=0 label_glyphs=0 text_center_offset=-37
icon consumed the label width or culled label glyphs
```

测试退出码为 1。第二项的实际 Text 中心向左偏 37 logical px；这也说明仅验证 wrapper 未越界不能证明内容正确显示。

## 修复与断言

仅将 Segmented 内部两种 slot wrapper 的布局改为 `BoxLayout{}`，不再填满有限 max width。既有 `measure_item` 按 intrinsic icon 尺寸预留 icon + gap，再在剩余约束内测量 label；既有 `place_item` 以测出的内容组宽度居中。外部 block item 的实际等宽拉伸保持不变。

新增回归定位具体 label 的 Text，避免把 icon 的 glyph 当成 label。验证内容包括：

- 四个 fixture scale（1/1.25/1.5/2）、三种尺寸、300→220 resize，共 24 组分配；label Text 宽度和 glyph 数必须非零。
- icon 与 label 间隔 6 logical px；以真实节点尺寸计算的内容组及纯文本 Text 中心误差小于 0.001 logical px。
- 检查实际 glyph rectangle 与 clip 的交集、opacity 和 alpha，要求 label 有可见 glyph；内容组和纯文本的可见 glyph 中心误差小于 2 logical px，以容纳字体 bearing/atlas guard。
- 原有 wrapped rich label + icon 测试增加具体 label 的非零 Text 宽度与可见 glyph 检查；保留选项身份、icon slot 执行次数、keyboard/focus、Modal/Spin、生命周期与 Theme Material 局部失效验收。

同一默认复现场景修复后：`icon=14 label=39 label_glyphs=5 text_center_offset=0`。大尺寸为 `icon=16 label=43`，24 组的纯文本 Text 中心偏移均为 0。

## 验证结果

- 独立 `out/build/windows-msvc-headless`，MSVC x64 / Ninja Multi-Config，编译最多 4 jobs。
- Debug：Segmented public API / Theme / component **3/3**，1.30 s；[完整输出](p2-after-debug.log)。
- Release：相同三项 **3/3**，1.07 s；[完整输出](p2-after-release.log)。
- clang-format 22.1.3：`python scripts/format-code.py --check`，540 个 owned files，0 failures。
- `openspec doctor --json` healthy；全量 strict validate 67/67；`git diff --check` 通过。

这是平台通用 headless 证据，没有新增 native GPU、Linux、OS DPI、系统字体或人工 IME 结果。没有修改 Theme、token、scene/renderer ABI 或公共 API。
