# 有限 runner 与 meta 机制

2026-11-14，Windows / CPython 3.10.11。合并 runner、native resource 兼容层及 meta 后，父工作区实际运行标准库 unittest **95/95**：runner/comparison/native resources/font requirements/pilot declarations/protocol/observations/regions/oracle/text units/meta。`list --suite pilot` 实际返回三条完整 source-bound 定义；只是列举，不计 execution。

- 机制控制覆盖 0/1/2/3/4 与退出优先级、空选择、漏动作/图/complete、crash/timeout、旧 out/路径逃逸、严格 report/JUnit/index/replay 和不可信来源；报告明确 `mechanism_controls=true`。
- meta 正红绿要求独立实际差分分类；marker-only、环境非零、错层、reference/input/profile/source 漂移、遗漏 case/checkpoint、损坏 artifact、复跑仍红均拒绝。普通 faulted run 不 pass，meta 和 fault run 不 publish reference cache；缓存命中仍完整校验，未命中仍执行 Web。
- 真实 acceptance hook 沿用当前 factory token 与受控 Input 回调。后者的实际 CORE 回归已由本轮 native input CTest 验证；本阶段没有运行实际 Web/native pilot 或真实 padding 图像差分。

复用命令：`python -B -m unittest discover -s tests -p demo_differential_runner_tests.py`、`demo_differential_runner_comparison_tests.py`、`demo_differential_meta_tests.py` 及上述其余同名合同文件。原始日志、替身报告、图像和 manifests 仅在 ignored `out/`。

据机制与单位合同收口 4.1/4.5/4.6。4.2/4.4、实际字体/完整工厂输入、校准 profile、5.1–5.3/Windows paired 与故障红绿继续开放；planned 账本没有提升，不将 95 项机制测试称作 GPU 或组件视觉通过。
