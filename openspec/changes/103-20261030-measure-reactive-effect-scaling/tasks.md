# Tasks

worker 的 source-only 交接保留在 evidence/source-handoff.md。父线程现从 main `c7cb0950e5f66966180436f2e5d17b785a11350b` 进入正式构建与测量，按实测结果逐项勾选。平台通用合同选择 Windows MSVC/Ninja Multi-Config `windows-msvc-headless` Debug/Release，最多 2 jobs，测量期间不并发构建，不打开 GUI。

## 1. 平台通用：测量源码与有效输入

- [x] 1.1 准备 Reactive 五档 K、稳定/动态依赖、冷/warmup/hot CSV 与逐 observer 结果/销毁断言；通过正式 Debug/Release 新合同 CTest，并在 source-handoff.md 记录计时区、分配与不可观测计数。
- [x] 1.2 准备 effect 有效离散矩阵、geometry/material/mixed/overlap、共同事务与真实 bytes/ranges 观察、失败重试/epoch 输入；通过正式 Debug/Release 新合同 CTest，明确完整 effect sync 不是纯 planning。
- [x] 1.3 注册两个 HEADLESS 目标并保持 Core/common 边界；父正式 preset configure/build 与 include/link 守卫通过；执行限定新增文件格式检查、OpenSpec doctor/strict validate、git diff --check，核对交接文件。

## 2. 平台通用：真实 baseline

- [x] 2.1 父线程在同机 Release 完整矩阵至少运行三轮，保留每次 CSV、每轮 p95/分配/真实工作量及源码与二进制 SHA、工具链/preset/机器/参数；核验无等值、无退化单 range、无热 full-upload，并记录统计样本局限。
- [x] 2.2 分析 Reactive 与 effect 的扩展性和归因，分别记录是否构成热点；现有计数不足时先定义并实测最小私有 instrumentation，不用计时阈值或理论 K²/排序次数替代实测。

## 3. 平台通用：证据后的最小算法改造

- [ ] 3.1 只有 2.1/2.2 有证据支持时才选择保留稳定依赖或稳定句柄的最小 Reactive 改造，保持异常/销毁/动态依赖/重入合同并通过相应 CTest；若证据不支持改造，记录独审认可的不改决定，不能仅凭源码准备勾选。
- [ ] 3.2 只有 2.1/2.2 有证据支持时才选择共同 effect dirty 的统一合并或有序归并，保持两组并集、上传失败重试、owner/epoch 与 packed bytes 合同并通过相应 CTest；无收益证据时记录不改决定。

## 4. 平台通用：回归与独立审查

- [ ] 4.1 父线程执行正式 Debug/Release HEADLESS CTest（含现有 signal/memo/batch/scope/source lifecycle、rounded effect 和 scene backend 合同），至少三轮同口径前后测量并保存全部结果；不得用历史 100/101 或 Gallery 波动计算本变更收益。
- [ ] 4.2 独立源码/证据审查完成，检查工作量真值、异常/销毁/重入/失败恢复与不可观测项；整理实际测试命令、平台、SHA、残余风险后由父分阶段提交并立即安全推送，不主动 archive。

## 5. OS 与真实设备边界

此次测量不新增 OS 专属功能，无需为同一平台通用合同重复建立 Linux checkbox。真实 GPU、窗口、字体、IME/input/DPI、显示帧率均未被 HEADLESS/Recording 覆盖；若后续最小改造触及平台集成，先修订计划并按 Windows/Linux 分开独立验收。
