# Design

## Context

动机见 proposal。039/041 使用固定数组和 SliderRange 状态，已有受控 gesture/capture、候选 helpers、真实 label 和 persistent Tooltip。041 的内部 append_slot 提供可回滚动态挂载入口。已检查现有 header/host/tests，以及锁定 Ant Design 6.6.5 API 和发布的 rc slider 1.1.1 行为源码。

## Goals / Non-Goals

Goals：共用单值/双端/多端状态机；旧调用兼容；编辑时保持未变化端点的组件身份。Non-Goals：不引入 React/CSS/DOM 依赖，不建立平台 accessibility bridge，不新增 GPU ABI。

## Decisions

1. `MultiSliderProps` 使用 `SliderValues=vector<double>`；单值与 SliderRange 保持原公开类型。内部统一 sorted vector，默认 multi 为两个 minimum；显式空 vector 为零端点。count 上限 64 防止任意子树爆炸；`SliderRangeOptions{draggableTrack,editable,minCount,maxCount}` 原子配置，多端默认 0/64；双端仅提供 draggableTrack。
2. 固定数组改为稳定 heap thumb 记录，每个含组件/节点/interaction/surface、focus/hover 和提示 Signals。handler 捕获 generation ComponentId，再查当前索引，避免插入中间后闭包索引错位。数值变化不增删记录；count 变化按有序公共值匹配保留记录（含重合值），仅挂载新 thumb/销毁移除者。替代方案是全部 remount，会丢失焦点与提示。
3. 轨道按下在选中数值区间且配置允许时开始整体 gesture，保存起始列表与 pointer axis；移动对首端点候选对齐并限制共同偏移，再逐点归一化，保持规则 step 网格的间距。非规则 marks 可能改变间距，这是离散候选合同的结果；受控和非受控显示一致。marksOnly 不允许整段拖动，依 Ant 设计明确拒绝而非静默关闭。
4. editable 按下插入并捕获 rail，thumb drag 跨轴超过 130 logical px 显示删除预览，release 时提交删除；回到区域取消预览。删除以离散键盘命令立即完成，repeat/up 不重复；指针删除仍在 release 完成。当前 minCount/maxCount 和 rendered disabled 的组合每次先验证；限制改变取消 gesture。
5. 动态添加/删除的受控候选保存在 gesture vector；仅 value echo 改变显示 topology。删除/插入后的焦点按保留记录或相邻可用记录迁移；没有端点时不虚构焦点。value 外部 count 改变取消过期捕获；普通同 count echo 保留 gesture。回调复制后调用并重新查询根 ComponentId。
6. handleDisabled 的缺省项为 false，最多 64 项；全局 disabled 优先。任一 rendered disabled 时禁止 editable/track drag，rail/labels 就近查 enabled thumb。原生 SliderRef 持有可清理绑定，focus/blur 先核实 owner thread 和 component generation；cleanup 解除绑定，后续可复用。autoFocus 只作用于首次 mount。
7. hint placement 改为 optional，使既有 aggregate 中枚举位置仍可构造；未指定时按 orientation 选择 Top/Right。新增 adjustOverflow=true，传入现有 Tooltip。样式继续使用 Slider Theme token；删除预览使用既有 disabled 外观，不引入 renderer 路径。

## Risks / Trade-offs

- sorted scalar 值没有业务 key → count 改变按公共值稳定匹配；重复值顺序稳定，明确不承诺业务 key 语义。
- 捕获过程中删记录 → release 先解除捕获，更新记录前撤销过期事件；取消不提交删除，用真实 pointer/focus 合同验证。
- 稀疏 marks 整段平移不能保持所有间距 → 逐点候选归一化，记录行为并验证，与 step 网格场景区分。
- 暂态 Signals 与 topology → 完整配置先校验，挂载失败回滚新子树；不重跑父 slot。
- 本机仅 Windows → 逻辑用 windows-msvc-headless Debug/Release 一次验收，OS/GPU 用 windows-msvc Debug/Release；Linux native 单独待验。正式构建均 Ninja Multi-Config。

## Migration Plan

规划检查与提交后依用户授权立即 apply。先提交共用动态端点状态与兼容测试，再提交轨道/编辑/焦点/提示合同，最后 Gallery 与 Windows 原生证据。支持状态仅在相关任务真正通过后更新；无需数据迁移，按 commit 回退，不 push/PR/archive。

## References

- [Ant Slider API 6.6.5](https://github.com/ant-design/ant-design/blob/6.6.5/components/slider/index.en-US.md)
- [Ant Slider implementation 6.6.5](https://github.com/ant-design/ant-design/blob/6.6.5/components/slider/index.tsx)
- [rc slider 1.1.1 drag publisher artifact](https://cdn.jsdelivr.net/npm/@rc-component/slider@1.1.1/es/hooks/useDrag.js)
- [rc slider 1.1.1 editing publisher artifact](https://cdn.jsdelivr.net/npm/@rc-component/slider@1.1.1/es/Slider.js)
