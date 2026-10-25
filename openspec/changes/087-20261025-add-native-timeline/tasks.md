# Tasks

## 1. 平台通用 API、模型与 Theme

- [x] 1.1 实现 typed Timeline API、固定 key/order/placement 模型与独立 Theme 全链；以 model/public_api/theme CTest 验证非法输入、继承、精确 dirty、后代原子拒绝与恢复；新增 docs/timeline.md 的 API/Theme 合同。
- [x] 1.2 用本树新构建生成五份 goldens，深比较所有旧 typed 字段和值与旧 33 IIFE 原文/顺序及 identity ordinals，仅追加 Timeline；运行 theme_algorithm/runtime/public_api/default_theme 针对性测试后本地提交。

## 2. 平台通用 retained 组件与真实控件

- [x] 2.1 实现 vertical retained layout、regions、dot/tail/loading、visibility/reverse 与真实 Tab 顺序；以 component 测试验证文本、placement、tail、空/全隐藏 flow、Input/Button/Select 身份与草稿；补充文档和可编译示例。
- [x] 2.2 用 integration/region_clip 测试验证极窄/短/零高恢复的实际 glyph ink、surface/hit、focus decoration 与外部 hardclip；验证隐藏/Modal/Spin、controlled 拒绝 popup suppression、preedit/focus/capture、同步卸载/重绑与 owned block 清理，不影响 foreign owner；验证 idle 后本地提交。

## 3. 去重集成验收

- [x] 3.1 使用 Windows MSVC windows-msvc-headless Ninja Multi-Config、最多 4 jobs，运行 Debug/Release 全部 Timeline 测试及一次去重影响组合（Theme、Descriptions/Steps/Collapse、Select/Modal/Spin、focus/lifecycle/scene），记录实际目标、数量和边界，不执行历史全套或长 Gallery。
- [x] 3.2 运行 clang-format 22 `python scripts/format-code.py --check`、OpenSpec 1.14 doctor/strict validate、git diff --check；记录 evidence、精确 HEAD/branch/path/dirty 并本地提交，交父独立评审，不访问远端/push/PR/archive。
