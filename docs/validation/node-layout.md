# Retained Node and layout validation

验证日期：2026-08-26

本阶段验证 retained Node、Component mount、内部 Layout Engine 和最小 Dirty queue。`BoxLayout`、`FlexLayout` 仅位于 `src/layout/`，没有进入 `include/ryn/`，后续公开布局仍按 Ant Design 的 `Flex`、`Space`、`Grid` 和 `Layout` 语义设计。

| Contract | Evidence |
| --- | --- |
| generation-checked Node handle | slot 复用后 index 相同、generation 变化，旧 handle 查询失败 |
| Component mount | Component 执行 1 次；响应属性更新只修改 retained Node |
| Scope ownership | dispose 停止属性更新并递归卸载挂载 Node |
| Constraints | 非法和 NaN Constraints 在 Measure 前被拒绝 |
| Box/Flex | padding、fill、gap、horizontal/vertical 和溢出边界测试通过 |
| Material | color/opacity 只进入 Material queue，Measure/Place 计数不变 |
| Transform | translation 只进入 Transform queue，并保留 HitTest flag |
| Size | 进入 Layout root 与 Geometry queue，下一 pass 更新 Measure/Place |

Windows `windows-msvc` preset 的 Debug 与 Release 完整 CTest 均为 21/21 通过。
