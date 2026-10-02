# Design

## Context

动机见 proposal。039 内部固定一/二 thumb，typed limits 为 double step，已有受控 gesture、capture 和键盘合同。040 为窗口拥有的 persistent Tooltip 提供了独立布局和同帧定位。本 change 不改 renderer ABI，也不将 SDL 类型引入组件。

## Goals / Non-Goals

Goals：共享候选值算法，真实 Text 标签、独立 thumb 值提示和保留场景。Non-Goals：本阶段不重构多端点存储；整段拖动和动态多端点在后续收尾阶段继续，Web-only API 除外。

## Decisions

1. 保持 SliderLimits 的数值 step 和旧聚合初始化兼容，新增 marksOnly 属性表达 Web step=null 的原生语义。marks 使用 vector<SliderMark>{double value,String label}；一次验证完整 vector 后排序、提交。不加入 CSS 样式，mark/dot 外观通过 Slider Theme token。
2. 归一化用当前 step 候选加 marks/min/max 比较，二分找邻近 mark；dense step 不枚举。marksOnly 仅遍历/二分排序后的有限点。dots 开启时限制完整视觉点集为 4096，超限抛异常；默认不开启 dots，不影响旧输入网格上限。
3. marks 标签为 child component 的 Text，加独立点击 interaction；Slider 自有布局分别测量标签并预留文字区域，rail/thumb 位置只使用轨道区域。动态 marks 只重建 label 子树，thumb、scope 和 value hint identity 保留。空 label 保留点且不占文字空间。
4. thumb 外包 Tooltip，内部 trigger child 保持原有 hit/focus/scene。hint title/open 用 Signal；默认 Auto，hover/keyboard focus/drag 更新，禁用优先；formatter 默认用 locale-independent double 字符串。Tooltip 的 Escape latch 支持受控提示，通过 onOpenChange 收到关闭后锁存至 Auto 退出。
5. included=false 令 track 使用零尺寸 retained quad，避免纯切换重建 topology；dot 的 active 条件随 included 改变。新 metrics/colors 进入既有 Slider phase token，JSON/hash/继承/algorithm 和主题 golden 同步。

## Risks / Trade-offs

- limits 与 marks 的分别 reactive 更新暂不一致 → 每次更新验证组合，失败保留旧组件状态；调用者原子更新相关配置或先清 marks。
- label 布局会增加尺寸 → 空 marks 完全维持旧布局，明确标签预留空间、纵向和 reverse 合同。
- callback/formatter 可重入销毁 → 复制函数，在调用后重新查询 ComponentId；异常保留原 state。
- dense dots 开销 → 上限拒绝而非静默缺点；颜色/idle 保持保留对象，无组件 remount。
- 本机 Windows/MSVC → 平台通用合同只运行 headless Debug/Release，native Windows 验收 D3D12/DXIL/system fonts/input/scale；Linux 真实桌面专属项独立待验。

## Migration Plan

规划 doctor/full strict/diff 通过后独立提交，并依用户授权立即 apply。先提交数值合同，再提交标签/dots/hints/Theme/Gallery 及完整 headless 验证，最后保存 Windows GPU evidence。多端点阶段保持文档和 Gallery partial；无新增依赖，按提交回退恢复旧行为。
