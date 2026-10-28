# Tasks

## 1. 公开 API 与选择模型（平台通用）

- [x] 1.1 实现 typed 固定 TreeSelectItem、TreeSelectProps 与单值／开合模型；新增 API 编译、唯一 key／规模限制、受控拒绝／接受／重复选择／未知值测试和使用文档；MSVC HEADLESS Debug/Release 定向通过后提交。

## 2. 原生组合与 Theme（平台通用）

- [x] 2.1 实现 retained Input／Popover／Tree host、指针／键盘／清空／展开／有限滚动与布局；新增真实事件、三主题有限尺寸、同帧祖先定位、glyph/fill/hit/适用 focus coverage 测试，更新文档并实际构建运行 Debug/Release。
- [x] 2.2 接入独立 TreeSelect Theme 全链与子组件映射；新增继承／算法／override／dirty／identity／祖先失败原子性测试，生成五份 fresh goldens 并深比较所有旧 typed 值、原 IIFE 和 ordinal 前缀，检查默认 1 MiB PE 栈。
- [x] 2.3 完成 Form String/control(ref) reset、边界／零面积／Tab／Escape 恢复、回调销毁重绑及局部 catch 挂载异常静默回滚；验证只读 selector 的实际 session 门禁、资源／block 回基线和替代 generation，并在 Debug/Release 实跑；首版无搜索，不建立可编辑 preedit。

## 3. 合并验收（平台通用）

- [ ] 3.1 运行 TreeSelect 与直接受影响 Tree／Popover／Input／Focus／Form／Theme 组合测试，clang-format 22 检查、显式 OpenSpec 1.14 doctor/strict 和 diff check；独立 Astra xhigh/default 审核解决所有可达 P1/P2，记录精确产物、命令、范围并提交／推送 main、核远端 SHA。不得把 HEADLESS 记为 native GPU、Linux、系统 IME／AT 或 Gallery 验收。
