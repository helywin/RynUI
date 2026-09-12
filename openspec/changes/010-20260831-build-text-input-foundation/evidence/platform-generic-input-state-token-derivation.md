# 平台通用：Input 状态 Token 派生子阶段

2026-09-12，任务 7.1 的状态 Token 派生已实现；Input 图层消费和状态动画属于紧接着的子阶段，当前不勾选 7.1/7.2，也不把这些颜色/阴影在 catalog 中提升为 runtime。

## 来源

- Ant Design 6.5.0 checkout `740ad964dc2397f33e40944367b0536a7314cc32`：`components/input/style/token.ts`、`variants.ts`、`components/theme/themes/shared/genColorMapToken.ts`、`themes/dark/colors.ts` 和 `util/getAlphaColor.ts`。
- Context7 结果中的 master 链接仅作导航；实际合同以以上固定 checkout 为准。
- 独立只读参考缓存中的 npm `@ant-design/colors@8.0.1` archive SHA256：`79758b1c7064ca63e40f935e597f72ad512bc68df42df0a6b65ca7df612dda4d`。
- 配套 `@ant-design/fast-color@3.0.1` archive SHA256：`873cab55c5f0dac78351bc10654009d9ddc71fcf627ca4a8f6f5743ef5c2b77c`。它们仅用于执行参考计算，未加入产品依赖或修改锁定版本。

## 已实现

内部颜色集解析 outlined default、hover、active、disabled、error、warning、placeholder，以及 RynUI desktop selection/caret 映射；error/warning hover border 使用 palette key 4，不能误用 hover key 5。三类 active ShadowList 使用上游 alpha 重建与 `2 * lineWidth` spread。custom primary 使用 palette 算法，不使用线性白色混色近似。

typed Input overrides、snapshot equality/hash、diagnostic JSON、nested inheritance、新增 `Input.colors` 与 `Input.shadows` 订阅已覆盖。颜色变更只产生 Material；同层数但不同颜色的 shadow 仍改变 identity 并通知 geometry/material，不触发布局。

直接执行参考包与 alpha 公式后，测试锁定以下 outline RGBA：

- Default primary `(5,145,255,0.10)`、error `(255,38,5,0.06)`、warning `(255,215,5,0.10)`。
- Dark primary `(0,60,180,0.15)`、error `(238,38,56,0.11)`、warning `(173,107,0,0.15)`。
- 自定义 purple `(114,46,209)`：hover `(146,84,222)`，outline `(155,5,255,0.06)`。

五份 golden 仅新增 Input 状态字段及更新 identity，程序化核对既有全局和尺寸字段完全不变。

Windows/MSVC `windows-msvc-debug` 构建成功，Token、Theme、Input component/GPU 与公开依赖相关 15/15 CTest 通过（8.97 秒）。这是 Token 派生合同，不是图层消费、动画或原生窗口的完成证据。
