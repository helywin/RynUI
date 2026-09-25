# Tasks

## 1. 规划与基线

- [x] 1.1 完成 proposal、spec、design、tasks；运行 `openspec doctor --json`、`openspec validate --all --strict --no-interactive`、`git diff --check` 并英文提交规划
- [x] 1.2 添加固定 seed 的局部更新规模 benchmark，使用 Windows MSVC Release preset 在改造前记录 Node 数、脏量、脏区数、atlas entry 数、CPU 时间与源码 SHA；定向测试通过后英文提交

## 2. 平台通用局部更新实现

- [x] 2.1 实现每 domain generation-aware slot stamp；补充重复入队、首次顺序、slot 重用、clear 后重新入队及根语义测试；Windows MSVC Debug 构建与定向 CTest 通过后英文提交
- [x] 2.2 Quad/Glyph 脏区改为 append 与读取时一次归并；补充乱序、重复、相邻与变长 replace 测试；Windows MSVC Debug 构建与定向 CTest 通过后英文提交
- [x] 2.3 Glyph atlas 增加完整 key 索引；补充 phase/mode/字体区分及失败后重试测试；Windows MSVC Debug 构建与定向 CTest 通过后英文提交

## 3. 通用验证与收口

- [ ] 3.1 Windows MSVC Release 重新运行同一 benchmark，记录改造前后 CPU 结果、平台/preset、工作量和未测 GPU 边界；运行受影响 CTest 与 `git diff --check` 后英文提交证据
- [ ] 3.2 运行 OpenSpec doctor、strict validate、受影响完整 CTest；核对所有 task 的实际证据并英文提交最终状态
