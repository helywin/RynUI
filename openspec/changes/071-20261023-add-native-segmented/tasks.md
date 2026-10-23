# Tasks

## 1. 规划

- [x] 1.1 创建精简规划，运行 doctor、全量 strict validate 与 diff check，通过后本地提交。

## 2. 平台通用实现

- [x] 2.1 实现 public API、tokens、retained host、最小 services/CMake 注册和使用文档；以 windows-msvc-headless Debug/Release 定向 tests 验证 controlled refusal/default、响应属性、实际横纵/等宽几何、键盘焦点、富标签与 icon、Modal/Spin、销毁安全和纯颜色局部失效，通过后本地提交。

## 3. 平台通用收口

- [x] 3.1 运行旧 Theme 定向回归、所有旧 golden 字段比较（仅忽略 identity）、format check、doctor、全量 strict validate、diff check，记录 MSVC preset 和真实范围，提交证据并交付干净分支。没有 native GPU、Linux 或人工 IME 验收声明。

## 4. P2 内容测量修复

- [x] 4.1 先用实际 text 宽度、glyph 可见性与内容居中断言复现 icon/label 填充错误，再将 slots 改为受约束的 intrinsic wrapper 测量；验证 300 宽双选项 block、纯文本、富标签/图标、resize 与既有交互在 MSVC headless Debug/Release 通过，更新 071 evidence，并通过 format、doctor、全量 strict validate、diff check 后本地英文提交。

## 5. P2 受限高度内容裁剪

- [x] 5.1 先复现多行 label 的可见 glyph 越过 item，再裁剪 slot 内容到实际分配区域且保留 row focus/shadow；增加受限横纵布局、短/零剩余高度与 rich icon+label 的可见 glyph containment 断言，完成 MSVC headless Debug/Release 定向测试、format、doctor、strict validate、diff check，更新 071 evidence 并本地英文提交。
