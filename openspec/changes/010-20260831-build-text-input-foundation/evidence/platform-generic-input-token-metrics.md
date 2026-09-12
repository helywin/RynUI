# 平台通用：Input Token 尺寸子阶段

2026-09-12，任务 7.1 的已实现子集；完整 7.1 保持未勾选，颜色/阴影、typed Input overrides、独立 Token identity 和生成目录支持仍待后续实施。

已读取 Ant Design 6.5.0 的 `components/input/style/token.ts`、default/compact size map 源码，并核对 checkout 为 `740ad964dc2397f33e40944367b0536a7314cc32`。

- 新增 internal `InputTokenSet` 与三档尺寸解析；small 使用 base font，large 使用 `fontSizeLG * lineHeightLG`。
- default small/middle/large：高度 24/32/40，字体 14/14/16，行高 22/22/24，inline padding 7/11/11，block padding 0/4/7。
- Compact：高度 21/28/35，字体 12/12/14，行高 20/20/22，inline padding 7/7/11，block padding 0/3/5.5；affix gap 仍为 4，不是旧实现中的 2。
- Input layout 直接消费 block padding 并在受限高度内收缩；Text 组件独立字体尺寸 override 不再改变 Input 自身尺寸。字体 family/weight 与状态材料迁移尚未在本子阶段改动。
- 浮点舍入按上游 `Math.round`/`Math.ceil` 公式实现，小数 controlHeight=33.3、lineWidth=2 的 middle block padding 为 3.6；使用 Node 执行原公式核对，未用十进制直觉替代运行值。

Windows/MSVC `windows-msvc-debug` 构建成功。Input source contract、Input component/GPU、Theme algorithm/scope、layout API/engine/allocation/style/demo/evidence 共 13/13 CTest 通过（3.32 秒）。包含 Default/Dark/Compact、custom seed、nested inheritance、inherit=false、错误 size 与实际 Input viewport 尺寸断言。这里不是原生窗口视觉验收。
