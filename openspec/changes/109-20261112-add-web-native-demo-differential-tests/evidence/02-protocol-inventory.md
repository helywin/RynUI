# 协议与实际 inventory

2026-11-12，Linux/GCC，实施基线 `a45a83e85524e3662fd098daf34bbb9cd3af2942`；规划以独立提交 `e33095448604cf03c1831c0afc77dbbb706352db` 接入。任务 1.1–1.5 已实施，尚无 Web/native 差分或 GPU 通过声明。

- 六种 schema v1、有限 action/oracle grammar、标准库严格 validator、有序 writer/binary join、身份保留合同和声明性 pending ledger 已接入。非法字段、非有限值、重复/回退 step、无 oracle、native-derived expected、危险路径、同 count 换身份、重排、owner/alias 错误和新身份漏映射均有自动负例。
- main 自有正式 `linux-gcc-headless`，`rynui_demo_differential_tests`，单次 `-j2`；最终 Debug → Release 串行构建成功，`ctest --preset linux-gcc-headless-{debug,release} --no-tests=error -R '^rynui\.demo_differential_'` 各 **2/2 通过**（15 个协议/inventory unittest，binary 检查无跳过）。未重跑全量 CTest。
- `run_demo_differential.py inventory` 实际无显示环境比较编译 registry 与 writer：catalog 73、routes 80（77 组件/家族含两说明页 + 3 内建；旧 native visual 口径为 75）、samples/sources 114/114（110 组件 + 4 Token）、targets 490。完整身份/顺序/owner/source 差量为空，保留 Card rounded-mask、Modal backdrop 和既有目标。757 skeleton obligations 全部 pending，**零执行 pass**；这些数量不是未来上限。
- schema 源一致性、clang-format **22.1.3**（1216 个 tracked source；新增 source 已显式格式化）、`git diff --check` 通过；任务私有 OpenSpec **1.14.0** doctor healthy、strict **109 passed / 0 failed**。系统全局 1.14.3 与生成 skill 未改。

HEADLESS 的编译 registry 和协议结果不证明 Web 执行、三例 pilot、视觉比较、校准、真实 GPU readback/present、Windows/MSVC/D3D12 或系统 IME/AT。原始 inventory、源码 excerpt、构建/CTest/工具日志全部在 ignored `out/demo-differential*`；本提交只有可复用代码、确定性声明/schema 和本摘要，不修改 107/108 任务向量。
