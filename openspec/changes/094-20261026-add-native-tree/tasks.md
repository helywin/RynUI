# Tasks

## 1. 规划与基线

- [x] 1.1 锁定本地 Ant Design 6.6.5 Tree 参考与当前基线，完成简中规划；通过 OpenSpec 1.14.0 doctor、strict validate 和 git diff --check，创建本地规划提交。

## 2. 平台通用模型与 Theme（本机 Windows MSVC HEADLESS）

- [x] 2.1 实现固定拓扑、显隐、展开、单选、非 strict 级联勾选与 disabled 隔离；模型测试覆盖受控候选与边界拒绝，并记录 API 文档。
- [x] 2.2 追加 Tree Theme 全合同；Theme 测试验证五种算法 fresh goldens、旧 typed 值/IIFE/ordinals、字体边界及继承原子失败恢复；格式检查通过后本地提交。

## 3. 平台通用组件（本机 Windows MSVC HEADLESS）

- [x] 3.1 实现保留 slots、原生指针和 roving 键盘、有限 viewport 滚动与 scene；定向组件测试验证受控/默认、富 Input/Button/Select 草稿/ref/身份、显隐和折叠恢复、有效富 child、邻居 Tab、实际 glyph/fill/hit/focus 裁剪。
- [x] 3.2 覆盖 mount/Prop/theme/输入边界重入销毁 owner 与私有节点、代际隔离、popup/capture/focus/session/preedit 清理及 owned/foreign blocks；通过生命周期测试和直接共享回归，提供可编译 docs/tree.md 示例，创建本地提交。

## 4. 集成收口

- [x] 4.1 在自身输出目录通过 windows-msvc-headless Ninja Multi-Config Debug/Release 定向 CTest 与共享回归；保存 configure/build/test 原始日志、fresh goldens 和准确边界，运行 format 22.1.3 check、OpenSpec doctor/strict validate、diff check。

## 5. Windows

- [x] 5.1 验证本次 MSVC PE 仍为默认 1MiB 栈；提交最终证据后交接 clean HEAD/branch/path 与实际测试数。原生窗口/GPU、系统 IME/AT 和 Linux 不在本次验收范围。

## 6. Fresh Astra R1 修复（既有范围，Windows MSVC HEADLESS）

- [x] 6.1 对局部 catch 后挂载残留、已布局零面积输入、父滚动 checkbox 重复 translation、无 range 滚轮吞事件逐项真实复现，保存四份原始 before 失败日志并本地提交回归。
- [ ] 6.2 用完整 generation 的挂载事务回滚覆盖 getter/slot/订阅/首次协调异常，验证原异常、fallback 与替代 generation/ref、资源及订阅清理。
- [ ] 6.3 修复已提交零面积布局的整树输入边界、checkbox 单次 translation、inline wheel handoff，验证恢复身份、真实场景坐标/coverage、Modal 和 child popup 优先级。
- [ ] 6.4 每配置一次运行全部既有 Tree、新回归和直接受影响代表（最多 2 jobs），保存 before/after/build 原始日志；验证 format 22.1.3、OpenSpec 1.14.0 doctor/strict、diff、旧 IIFE/ordinal/golden 不变与默认 1MiB PE 栈，交接 clean 本地 HEAD 待父 fresh Astra R2。
