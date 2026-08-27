## Why

`004-20260827-build-button-interaction-foundation` 已建立公开 Text/Button 与完整输入闭环，但示例仍由内部 `FlexLayout` 手工组织，应用无法使用稳定的公开布局 DSL 组合页面，也无法证明方向、对齐、换行和间距变化只触发必要布局。下一步需要发布 Ant Design 6 风格的 `ryn::Flex` 与 `ryn::Space`，为结构响应、Scroll 和 Device Monitor MVP 提供统一的公开容器边界。

## What Changes

- 增加公开 `FlexProps`、typed content slot 与 `ryn::Flex`，首批覆盖 horizontal/vertical、wrap、主轴 justify、交叉轴 align、preset/custom gap、reactive `Prop<T>` 与 `LayoutStyle`。
- 扩展内部 Flex layout，使测量和放置支持多行 wrap、start/center/end/space-between/space-around/space-evenly、start/center/end/stretch，以及不同主轴/交叉轴 gap；布局顺序保持声明顺序，不增加视觉或交互 wrapper。
- 增加公开 `SpaceProps`、typed content slot 与 `ryn::Space`，首批覆盖 horizontal/vertical、wrap、start/center/end 对齐、Small/Middle/Large 或自定义双轴间距；Space 以稳定 item identity 保持相邻内容等距，但不向 consumer 暴露内部 wrapper。
- 扩展 `LayoutStyle` 的 flex child 字段，首批覆盖 grow、shrink、basis、align-self 与 order；这些字段只影响组件在父 Flex/Space 中的外部布局，不改变稳定组件视觉。
- 将 Ant Design 6.5.0 的 gap preset 锁定为 Default Theme snapshot 中的 8/16/24 logical pixels，并通过 token contract、布局矩阵、响应式最小失效、公共 API 和真实窗口示例验证。
- 分离 Linux 与 Windows 构建、CTest、真实窗口截图和 evidence 清单；任一平台的结果不得替代另一平台。
- 非目标：Grid/Row/Col/Layout、Scroll/Clip、`If`/`For`、RTL、baseline、align-content、reverse 方向、`Space::separator`、`Space::Compact`、公开 Theme override、任意 CSS 字符串或通用 `Modifier`。

## Capabilities

### New Capabilities

- `layout-containers`: 定义公开 Flex/Space typed Props 与 slots、flex child `LayoutStyle`、方向/对齐/wrap/gap 的测量放置、响应式失效、生命周期和跨平台验收合同。

### Modified Capabilities

无。当前尚无归档到 `openspec/specs/` 的布局 capability；本 change 的公开组合与 `LayoutStyle` 扩展统一在新 `layout-containers` capability 中定义。

## Impact

- 新增 `include/ryn/` 下的 Flex/Space 公开 API 并从 `rynui.hpp` 导出；扩展 `src/layout/`、`src/component/`、Default Theme snapshot、示例和测试。
- 复用现有 ComponentHost、Node/Scope identity、typed content、`Prop<T>`、Dirty queue 与 paint traversal；普通 gap/alignment 更新不得重新执行 content slot或重挂 children。
- 不新增第三方依赖；继续使用锁定的 SDL3、FreeType、HarfBuzz 与 shader toolchain，公开 header 不得泄漏内部 Node/Layout/SDL/GPU 类型。
- 主要风险是有限约束下 flex grow/shrink 的确定分配、wrap line break 与 child intrinsic measurement 相互影响、Space item identity 扩大场景/HitTest 顺序、order 与声明/focus order 分歧，以及响应式字段更新错误扩大为 Structure dirty。
- 可验证结果是同一个公开 Flex/Space DSL 在不同窗口宽度下稳定重排，布局矩阵与最小失效由自动测试证明，并在 Linux 与 Windows 的独立清单中分别保存真实窗口证据；规划完成不代表功能已实现。
