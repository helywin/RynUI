# Design

## Context

动机见 proposal。基线为 `62c990a1b494532c358ac487b2bb5931c08a7231`。参考离线 Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd` 的 descriptions 文档、Row/Cell、hooks/useRow 和 style/index.ts。现有 Collapse/Form 已提供 retained slot、semantic text style、ComponentLayout 和 Theme 分组订阅模式。

## Goals / Non-Goals

目标是可直接用于详情页的原生多列面板，普通更新不重挂子组件。非目标见 proposal；不引入通用 table 或新的布局框架。无 renderer/SDL 或新依赖。

## Decisions

1. 固定 `std::vector<DescriptionsItem>`，构造时检查 key。item 持有 typed label/content，Props 持有可选 title/extra；`column`、span、visible、labelPlacement、bordered、size、colon 使用 `Prop<T>`。`DescriptionsSize::Default/Middle/Small` 对应上游 large/medium/small。`layout` 仅供外部 LayoutStyle。不采用动态列表 diff，避免无当前需求的结构框架。
2. 独立小 packing model 接受正列数、正 span 和显隐状态，按声明顺序填行；超过剩余列数时截断并结束当前行，最后一行末项扩展至行尾。隐藏项不占列，恢复沿用原 key/子树。数值非法时拒绝更新，不发布半成品配置。
3. 每个 item 是非交互容器，label/content/colon 分开保留，外层 ComponentLayout 测量和摆放。列宽为可用宽度的等分，span 合并；水平 bordered 标签占一个半列单位，值占余下 `2*span-1` 单位；水平 unbordered 标签以内在宽度且最多半个 item 宽度分配，其余给值；vertical 每个逻辑行的标签和值分别对齐最大高度。有限宽度二次测量以获得真实换行。无界宽度先求自然列宽再在有限宽度测量，不向 scene 发布无穷。
4. label/content/title/extra slot 对自身分配区启用 content/hit clip，item 和根保留外层有限 clip。内部 `ClipPurpose` 区分内容与外扩装饰，节点默认同时裁剪两者，仅 Descriptions 的被动 slot 不限制 outward decoration；Input/Button 通过同一祖先链的 decoration 查询保留焦点环，Modal 的 fill 与 decoration 也分开查询。不改变子控件测量、摆放或 GPU ABI。隐藏分支通过现有 shared boundary helpers 取消输入、关闭 popup，独立于焦点。所有 owned input block 在 resource cleanup 注销。callback/focus 可能卸载组件，跨调用后使用 id 重新查询。普通 Theme 更新不重设 draft。
5. Descriptions 独立 colors/metrics token，包括三种 padding、字型、标题、间距、边框；Theme 算法关闭时忽略 component seed。最终字体和 metrics 在 snapshot 发布前验证：有限、非负，字体至少 0.5 logical dp 且可 lround；组合尺寸无溢出。新增 identity 只附加；保留旧 28 段 IIFE 顺序/文本和全部旧 golden typed values，沿用 heap/segregated resolve，不增加 stack。
6. surface 只写共同 logical CPU scene。颜色订阅只触发 material，metrics/属性布局变化通过 DirtyQueues 唤醒，idle 无自主帧或动画。丰富子内容按原生输入合同工作，不给容器注册禁用交互祖先。

## Risks / Trade-offs

- 极短/零空间 → 每层分配非负有限矩形，实际 glyph/surface/hit clip 测试及恢复测试。
- 保留隐藏编辑器的清理 → owned boundary cleanup、捕获/焦点/Select/Modal/Spin 和 teardown 专项测试。
- Theme 体积和继承失败 → 单独 derive helper、预发布验证、后代拒绝及恢复，并比较新可执行文件生成的五组 golden。
- 等分原生列不复制 Web auto table 算法 → 文档明确首次发布的确定性分配规则。

## Migration Plan

新增 API，无旧调用迁移。分三次实施提交：API/model；retained host/Theme；专项集成/docs/evidence。另有先行规划提交。父代理负责独立 Astra review、集成及远端操作。本工作树只本地提交。

验证使用 Windows MSVC `windows-msvc-headless`，Ninja Multi-Config，Debug/Release，最多 4 jobs。平台通用逻辑只在本机验收一次；无 GPU、Linux、系统 IME、AT 或 Gallery 验收声明。
