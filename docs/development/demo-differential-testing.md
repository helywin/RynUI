# Web / native demo 自动差分

109 的唯一入口是 `python -B tools/run_demo_differential.py`。协议与覆盖基础已实施；Web/native 执行、比较、校准及真实 pilot 仍按 109 任务追踪，不能把 inventory 校验称作视觉或行为通过。工具不以人工或 Agent 读图裁决。

## 有序 inventory 与分母

```text
cmake --preset linux-gcc-headless
cmake --build --preset linux-gcc-headless-release --target rynui_demo_differential_tests -j2
python -B tools/run_demo_differential.py inventory --binary out/build/linux-gcc-headless/tests/Release/rynui_demo_differential_tests --output out/demo-differential/inventory-check-1
```

`--output` 必须是当前 checkout `out/` 下尚不存在的目录。Gallery 本身也接受独占参数 `--page-inventory=<out/file.json>`，在创建窗口、字体、GPU 或帧泵之前返回；不需要 display、compositor 或 Web 安装。HEADLESS 测试程序链接同一个生产 registry/exporter，检查编译时数据与当前源码 writer 的完整有序向量，而非数量。

当前声明基线 `a45a83e85524e3662fd098daf34bbb9cd3af2942`：

| 向量 | 当前数量 | 口径 |
| --- | ---: | --- |
| catalog | 73 | canonical 组件，Input 仅一项 |
| routes | 80 | 77 个组件/家族合法路由（含 List、Util 说明页），另 3 个内建路由 |
| native component/family routes | 75 | 上述组件路由去除 List、Util；与旧 visual discovery 的口径一致 |
| samples / sources | 114 / 114 | 110 个组件样例，另 4 个 Token 样例 |
| legacy targets | 490 | 每项保留实际 sample owner 与 replacement alias |
| coverage skeleton | 757 | catalog、route、sample 可见区、target 各自独立 obligation |

这些是冻结基线的观察值，不是未来上限。`identity-baseline.json` 是已声明的身份/顺序/owner/source 合同，特意不存运行结果、截图或源码副本。新增身份立即形成 coverage 缺口；删除、同 count 换身份、重排、owner/source 迁移需要明确修订该合同，不能调整数量或通过子集检查消除。baseline 保留全部现有 target，含原始迁移目标与后增身份。support status 的独立文案变化不构成身份迁移。

实际 `inventory.json` 包含编译后的 source excerpt，留在 ignored out；工具重新运行 `generate_gallery_source.generate()`，逐项验证其函数、公共 header、真实代码及顺序。source 行号/代码改变而未重建也会拒绝。catalog 内容摘要同时核对 source manifest、overlay 和两种既有 schema。alias 必须无环、保持同一 sample owner，或由该 owner 的真实 factory 消费新目标身份。

`coverage.json` 是声明性映射，当前全部 `pending/planned`。`implementation=ready`、映射类型和本次 execution 是不同轴；planned、缺 case、not-run、fail/error 均不计 pass。List/Util 仍须说明页合同，不能据此豁免其他已有 native 能力。后续 feature matrix / case 映射将在 109 的对应阶段扩充，现有 skeleton 不声称所有 feature 已实现。

## 协议维护与验证

六种 version-one schema 位于 `tests/fixtures/demo-differential/`，schema 源定义在 `tools/demo_differential/schema_definitions.py`。只存 schema、声明性输入和映射。修改 schema 源后执行：

```text
python -B tools/demo_differential/schema_definitions.py
python -B tools/demo_differential/schema_definitions.py --check
python -B -m unittest discover -s tests -p 'demo_differential_*_tests.py'
ctest --preset linux-gcc-headless-debug --no-tests=error -R '^rynui\.demo_differential_'
ctest --preset linux-gcc-headless-release --no-tests=error -R '^rynui\.demo_differential_'
```

当前协议基础仅用 Python 标准库，不安装或隐式发现第三方依赖。内置 validator 仅解释这些 schema 实际使用的 JSON Schema 2020-12 vocabulary，遇到未知 vocabulary 失败；不承诺任意外部 schema 的完整实现。JSON 限制为 16 MiB、48 层，重复字段、非有限数字、未知协议字段、危险相对路径、重复身份/step、时钟回退、无 action 的 checkpoint、缺 oracle 和 native-derived expected 均拒绝。公开 Props/数据等领域 map 允许声明的 JSON 数据，不接受可执行 grammar。

所有原始 inventory、报告、日志、源码副本、图片与二进制只写 ignored out。上述自动测试验证协议/覆盖与 CPU 编译 registry，不证明 Web、GPU readback、真实呈现、Windows 或系统 IME/AT。
