# Design

## Context

见 proposal。Tree 通用 refresh 构造 shown/normalized selection/checks 并逐行反复线性 membership，无条件标记 scene structure dirty；Table refresh 每次读取 source_order 记录并 stable_sort。二者都有既有代际、焦点、富后代和回调销毁守卫，必须保留。

## Goals / Non-Goals

**Goals:** 把局部材质变化与结构/排序输入分开，缓存只按真实依赖失效，消除重复排序和逐行线性 membership。

**Non-Goals:** 不依赖虚拟化，不改公开 API、Theme、排序图标或 Gallery；不通过禁止回调重入规避现有合同。

## Decisions

1. Tree 使用固定拓扑索引表达 shown/selected/check membership；纯 hover 只更新旧/新行呈现，focus 更新 roving 入口和必要旧/新行，不调用全树 projection。展开、disabled、勾选、Theme 等真实输入继续按对应依赖更新。
2. 结构 dirty 由实际可见结构、子图元激活或顺序变化决定；不能以删除全部 structure 标记换性能。回调后的完整 generation/intact 检查继续保留。
3. Table 区分数据/排序结果与分页切片。接受的新数据/排序触发重建，分页复用排序结果；selection、bordered、纯颜色更新不得调用 comparator。使用已有 key/索引表加速 visible/selected membership，避免第二套通用缓存框架。
4. 用稳定计数证明局部性：Tree 128/512/1024 节点；Table 1k/5k/10k 行 comparator、访问量、分配和 p95。测试回调更新、销毁/重绑、受控拒绝、分页、disabled conduction、富 editor 草稿及完整 generation。所有规模记录三轮，保持原始数据。
5. worker 只在隔离 checkout 修改 Tree/Table 私有文件与专项测试；构建、CMake 合并及计数运行由父线程集中安排。正式 Windows MSVC/Ninja Multi-Config HEADLESS Debug/Release，最多 2 jobs；无 GUI 操作。
6. 父集成回归发现的失败边界随本阶段修复：排序成功先标记 order dirty，完整行顺序在临时 vector 成功后才交换；异常保留尚未完成的 geometry 请求，等待下一次正常刷新。富单元格 Text getter 销毁 owner 后，内容交付先核对原 TextSceneId 完整代际，避免写入已退役场景；不改变普通 Text 交付或公开 API。
7. 首轮实测发现分页仍触发全表 interaction reorder，其逐项去重/查找为二次方工作。Table 按完整 sorted_order 一次排列所有 retained 行后代，只在数据/排序变化时重排；分页通过既有 branch eligibility 跳过隐藏行。直接验证声明顺序保持、实际 Tab 路径、外部邻居及草稿，不修改共享 InteractionRegistry 算法。

## Risks / Trade-offs

- 漏失效产生陈旧排序 → 明确记录 data/sort/page 依赖，并测试同 key 新记录、增删、受控接受/拒绝。
- 快速路径跳过必要输入清理 → 保留 disabled/hidden/零尺寸、Modal/Spin、销毁回调和富控件测试。
- 富行顺序改变 Tab → 复用实际 logical descendant 排序，检查外部邻居顺序和完整代际身份。
- 基准结果被分配/字体首帧污染 → 分开冷挂载与热更新，禁止并发 build/perf，按因果计数验收。

## Migration Plan

Tree、Table 两个小阶段独立验收提交；无公开 API 迁移。父线程完成 main 交叉回归、独立审查后统一更新审查文档，不能只凭源码 handoff 勾选完成。
