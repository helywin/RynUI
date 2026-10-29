# Spec Delta

## Purpose

使固定拓扑 Tree 在大型节点集合中保持局部交互成本，同时延续展开、级联勾选、禁用隔离、富内容编辑和键盘导航的既有语义。状态优化必须保留实际呈现、命中以及完整代际身份。

## ADDED Requirements

### Requirement: Local Tree interaction does not rebuild structural projection

系统 SHALL 将纯 hover/focus 变化限定在相关行状态，不重新投影整树或无条件重建 scene 结构。真实展开、禁用和勾选变化仍须完整更新受影响状态。

#### Scenario: Pointer and keyboard move between existing rows
- **WHEN** 在 128、512、1024 节点的固定拓扑中移动 hover 或键盘焦点，且结构状态不变
- **THEN** 仅旧/新相关行发生呈现变化，无整树 projection 或 composer 结构重建，焦点与实际命中仍正确

#### Scenario: Structural state changes after a local update
- **WHEN** hover 后改变展开、disabled 或级联勾选状态
- **THEN** 可见性、级联结果及输入隔离正确，富 slot 草稿与存活节点的完整 generation 保持
