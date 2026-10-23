# Tasks

## 1. 平台通用：规划

- [x] 1.1 保留 066，核对架构、相邻组件和 Ant Design 6.6.5 source hash，完成精简产物；运行 doctor、全量 strict validate 与 diff check 后提交推送。

## 2. 平台通用：Pagination

- [x] 2.1 实现 native Pagination、typed token、文档和 focused tests；在独立 Windows MSVC headless Debug/Release 构建树验证受控拒绝/回写、边界、总数收缩、禁用、键盘、主题和销毁，format/check/strict validate 后提交推送。

## 3. 平台通用：Table

- [x] 3.1 实现 typed retained Table、列内容、排序、多选、有限滚动与分页组合，附业务示例及 focused tests；MSVC headless Debug/Release 验证真实数据更新/重排/增删、无关 slot 不重跑、受控排序选择、翻页、clip、加载、主题和清理。
- [x] 3.2 在相同 Windows MSVC headless preset 运行 Debug/Release 完整通用 CTest、格式、doctor、全量 strict validate 和 diff check，记录准确证据、限制并创建本地提交。遵循用户最新指令，不再 push，父任务负责合并并推送 main。原生 GPU/Linux/人工验收交由父任务，不在本 change 声称完成。
