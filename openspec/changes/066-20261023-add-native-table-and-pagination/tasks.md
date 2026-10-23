# Tasks

## 1. 平台通用：规划

- [x] 1.1 保留 066，核对架构、相邻组件和 Ant Design 6.6.5 source hash，完成精简产物；运行 doctor、全量 strict validate 与 diff check 后提交推送。

## 2. 平台通用：Pagination

- [x] 2.1 实现 native Pagination、typed token、文档和 focused tests；在独立 Windows MSVC headless Debug/Release 构建树验证受控拒绝/回写、边界、总数收缩、禁用、键盘、主题和销毁，format/check/strict validate 后提交推送。

## 3. 平台通用：Table

- [x] 3.1 实现 typed retained Table、列内容、排序、多选、有限滚动与分页组合，附业务示例及 focused tests；MSVC headless Debug/Release 验证真实数据更新/重排/增删、无关 slot 不重跑、受控排序选择、翻页、clip、加载、主题和清理。
- [x] 3.2 在相同 Windows MSVC headless preset 运行 Debug/Release 完整通用 CTest、格式、doctor、全量 strict validate 和 diff check，记录准确证据、限制并创建本地提交。遵循用户最新指令，不再 push，父任务负责合并并推送 main。原生 GPU/Linux/人工验收交由父任务，不在本 change 声称完成。

## 4. 平台通用：0362721 评审修复

- [x] 4.1 修复 Table 垂直滚轮方向与 popup host 顺序；实际 wheel 验证下滚/上滚、保留 Select 的 popup 优先消费与滚动后 anchor 跟踪，MSVC headless Debug/Release focused tests 后本地提交。
- [ ] 4.2 同步保留行的逻辑交互顺序；验证 A/B → B/A、插入 Input 行位于外部 following control 之前，身份保持且真实 Tab 顺序正确，MSVC headless Debug/Release focused tests 后本地提交。
- [ ] 4.3 修复隐藏 Pagination 的外部布局占位与 Table 等高单元格；验证 Flex 单 gap、显式高度、动态恢复身份、短 Checkbox/Text 与多行内容垂直居中，MSVC headless Debug/Release focused tests 后本地提交。
- [ ] 4.4 运行规定 format、doctor、全量 strict validate、diff check 与相关 token/公开 API 回归，追加五项复现和验证证据；仅本地提交，不 push，不重复完整 119 项套件。
