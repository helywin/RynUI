# Design

## Context

动机见 proposal.md。InputComponentHost 已共享 editor、IME 会话、焦点引用、统计、Theme 和 affix，三个 text scene 共享一次 shaping。LayoutEngine 提供 ComponentLayout 扩展，TextEngine 已能按 LF 段落和有限宽度生成 TextLine；缺少的是编辑器多行模式及行布局光标映射。TextCaretMap 当前明确拒绝多段落/RTL，TextEditor 当前逐标量删掉 CR/LF。

参考固定 Ant Design 6.6.5 commit 4a39f54842eade4e565ab336ef6097cd7e723cdd 的 [TextArea](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/input/TextArea.tsx) 与 [Input 文档](https://github.com/ant-design/ant-design/blob/4a39f54842eade4e565ab336ef6097cd7e723cdd/components/input/index.en-US.md)，记录检索日期 2026-10-03。仅借鉴原生适用的尺寸、变体、计数和焦点合同。

## Goals / Non-Goals

**Goals:** typed TextArea 直接加入既有宿主，保留唯一 editor 与 IME owner，不以每行 Input 模拟。行布局、导航和 scroll 使用 logical 坐标，GPU 仍走 logical scene/stores、SceneResources/SceneBackend。

**Non-Goals:** 此 change 不改混合双向 shaping 合同；OTP 和 Typography 接入另开 change。它们仍是后续收尾项。无 Web DOM、CSS resize 样式或 renderer 私有上传。

## Decisions

1. TextEditorStore::create 增加默认为 SingleLine 的 TextEditMode；MultiLine 将 CRLF/CR 规范为 LF。初值、用户编辑、formatter 结果和 authoritative 回写使用同一归一化入口，模式在生命周期内固定。保留单行默认兼容，不用可热切换模式隐式改写历史。
2. TextCaretMap 增加测量行版本，TextCaretStop 保存行号和 baseline；为每条实际软折行根据 shaped cluster 计算 grapheme stop。软折行重复 byte 保留 upstream/downstream 两个位置，默认 at 选择 downstream；点击按行选最近位置，导航保持期望 x。保留旧单行重载和单行无分配路径。这里的导航是现有 LTR/CJK 合同，后续 bidi change 扩展方向。
3. TextAreaProps 继承 InputPropsBase，内部转换到带 TextArea 配置的 Input 挂载。默认 rows=4、wrap=true、resize=Vertical；TextAreaAutoSize 包含 enabled/minRows/maxRows，enabled 时内容驱动高度且不允许手工 resize。TextAreaRef 复用 InputRef 的线程/身份合同，公开别名仅表达组件意图。配置非法在分配前拒绝，reactive 非法值保持旧状态。
4. 使用内部 ComponentLayout 在既有三 slot 中定位：prefix 空、editable 填满内区、clear 停靠右上，计数在末行下方；有限宽度测量 wrap，autoSize 按实际行数限高，固定 rows 保留可滚动 viewport。Theme 只负责 padding/font/色彩；LayoutStyle 显式外部尺寸按既有约束优先。行高/字体/宽度变化只重测必要布局，不重建编辑器。
5. 跨行选择和 preedit 下划线生成每行 coverage，使用 retained content ranges 和共享 text views/clip，不拆字串重复 shape。垂直 scroll 与水平无 wrap scroll 由 caret reveal、wheel 和 Page 导航维护，偏移对齐 physical pixels 不重复 raster。resize grip 使用正常 PointerRouter capture 和逻辑尺寸，资格变更/卸载取消 capture。尺寸回调持有副本，在同步完成后按存活身份通知，可安全卸载。
6. Plain Enter 插入 LF，primary Enter 触发 onSubmit；Tab 正常焦点遍历，readOnly 可选择/复制/滚动，disabled 禁止输入与拖动。IME preedit 拥有导航，stamp/取消/输入区域沿用现有会话事务；多行 caret rectangle 确定实际输入区域。

## Risks / Trade-offs

- [软折行同一 byte 有两个视觉位置] → 明确 affinity 并测试 Home/End、上下、点击和 Shift 跨行；不得仅用逻辑排序推导二维命中。
- [autoSize 在宽度变化时循环测量] → 在 LayoutEngine 同一 generation 内测量和放置；高度只由一次有限宽度测量/外部约束决定。
- [长文跨行选择的范围规模] → 仅发布实际行 coverage，使用共同 content-range 上限与明确失败，保留单行热路径零分配验收；不把单行 benchmark 当作多行性能结论。
- [回调重入与卸载] → 生命周期身份校验、回调副本、延后通知，测试 onChange/onResize 和分支撤销。

## Migration Plan

按 tasks.md 顺序独立提交。平台通用测试在 Windows windows-msvc-headless Debug/Release 完成一次；正式 native 构建使用 windows-msvc/MSVC/Ninja Multi-Config。Windows 的系统字体、SDL input、D3D12/DXIL 和缩放/resize 使用真实本机窗口与 GPU readback；Linux linux-native/GCC 或 Clang 的窗口/GPU/input 由实际 Linux 机器独立验收。本机没有 Linux 证据时保留该项未完成。
