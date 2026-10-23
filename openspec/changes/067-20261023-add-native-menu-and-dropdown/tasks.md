# Tasks

## 1. 平台通用规划

- [x] 1.1 检查现有 popup/Modal、固定范围并创建规划；通过 doctor、严格 validate 与 diff 检查后提交。

## 2. 平台通用 Menu

- [x] 2.1 实现 typed Menu、retained 行、分组/Inline 子菜单、键盘与受控请求；同时交付 API 文档与选择/禁用/展开/identity/cleanup 聚焦测试，使用 windows-msvc-headless Debug 验证。

## 3. 平台通用 Dropdown

- [x] 3.1 实现 typed trigger、命令菜单、右键定位、受控请求、焦点与边界；同时更新文档并通过 Button onClick、clip、翻转、外部关闭、初始 Modal、nested popup 和回调卸载聚焦测试。

## 4. 平台通用集成

- [x] 4.1 windows-msvc-headless Debug/Release 完整 CTest、format-code --check、doctor、严格 validate、diff 检查；保存真实证据与明确限制，提交并记录本地 SHA。依最新用户策略不推送，由父任务合并和推送 main。不包含原生 GPU/Linux/IME 验收。
