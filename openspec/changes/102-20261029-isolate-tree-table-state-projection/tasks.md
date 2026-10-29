# Tasks

## 1. 平台通用：Tree 局部状态

- [x] 1.1 分离 Tree hover/focus 与结构投影、用索引 membership 消除逐行线性查询；MSVC HEADLESS Debug/Release 覆盖禁用、级联、富控件、焦点顺序、隐藏/零尺寸和回调退役，保留原断言。
- [x] 1.2 对 128/512/1024 节点完成三轮热交互计数与微基准，确认 hover 仅更新旧/新行且不发生无关 scene 结构重建；保存前后访问量、分配、耗时及精确命令。

## 2. 平台通用：Table 投影依赖

- [x] 2.1 按数据/排序/分页缓存投影并优化 membership；MSVC HEADLESS Debug/Release 验证选择/bordered 零 comparator、排序与分页失效、同 key 更新、增删、受控拒绝/重入及 retained editor/焦点身份。
- [x] 2.2 对 1k/5k/10k 行完成三轮选择/翻页计数与微基准，记录 comparator、分配、访问量及 p95，确认纯选择/bordered 不重新排序，附前后原始记录。

## 3. 集成与审查

- [ ] 3.1 main 集成后运行 Tree/Table 及受影响 TreeSelect/Focus/输入合同，format、OpenSpec 1.14 doctor/strict、diff 通过；独立审查后更新 TUNE-PERF-003/004 证据，提交推送并核对远端 SHA。
