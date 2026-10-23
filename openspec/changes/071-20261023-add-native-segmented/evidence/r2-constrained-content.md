# R2 P2：受限高度 slot 内容裁剪

本次从干净的 `b53e06f07fb39c01900bae891269fe22e7762b34` 继续，没有合并 root/main 或任何其他基线。生产变更仅为两个内部 slot wrapper 设置 `clip_content = true`。

## 先复现可见 glyph 泄漏

保留原生产源码，只加入回归测试。水平 block，width=300、height=32，选项为 `Alpha\nBeta\nGamma` 与 `B`，后面放置普通 Text。item 高度为 28，其纵向范围为 [2,30]；原 Text glyph 使用 document clip，第二行的可见 glyph 超过 item：

```text
glyph clipping rich=0 vertical=0 height=32 item_height=28
escaped visible glyph x=[50,60] y=[29,41] allowed_y=[2,30]
constrained Segmented glyph escaped its allocated slot/item
```

Debug component test 0/1，CTest 退出码 8；[失败原始输出](r2-before-debug.log)。断言使用 glyph 的实际 position/translation、shader clip、opacity/alpha 计算可见 rectangle，不以 Text 或 wrapper bounds 代替绘制范围。

## 修复与验收

slot wrapper 保留 intrinsic 测量，并将其后代内容裁剪到布局后的 wrapper bounds。label/icon 的 viewport 受 item 实际分配限制；Text 和 icon layers 从祖先链取得同一内容 clip。row 自身不设该内容 clip，因此它的 focus ring、selected shadow 继续使用外层 ancestor/document clip。

新增 20 个场景：水平/纵向 × 普通多行 label/包裹 Flex 的 rich label + SearchOutlined × 高度 32/12/4/0/96。逐个检查 label、第二项 Text 和 icon 各层的可见 glyph 位于实际 item/slot 分配交集内；剩余高度为零时没有可见 glyph，有可用高度时标签仍可见。恢复高度与切换排列不重挂载内容。

测试另检查 focus-visible outline 及非零 blur 的 selected shadow 仍可见，且 clip 高度大于 item 本身。原有 R1 的 24 组 intrinsic 宽度/居中场景，以及 keyboard/focus、Modal/Spin、回调销毁、Theme Material 局部失效仍通过。

- `windows-msvc-headless` / MSVC x64 / Ninja Multi-Config，独立检出的 `out/build/windows-msvc-headless`，最多 4 编译 jobs。
- Debug：Segmented public API / Theme / component **3/3**，1.42 s；[完整输出](r2-after-debug.log)。
- Release：同样三项 **3/3**，1.26 s；[完整输出](r2-after-release.log)。
- clang-format 22.1.3：540 个 owned source files，0 failures。
- `openspec doctor --json` healthy；`openspec validate --all --strict --no-interactive` 67/67；`git diff --check` 通过。

本次没有改变公共 API、Theme/token、renderer/scene ABI、输入或生命周期机制。只有平台通用 headless 证据；没有新增 GPU、Linux、OS DPI 或人工 IME 结果。没有 push、PR、archive、子任务或 root/Gallery/global docs 修改。
