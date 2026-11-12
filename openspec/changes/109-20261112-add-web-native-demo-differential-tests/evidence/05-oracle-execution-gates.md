# 独立 oracle 与执行完整性

Linux、CPython 兼容脚本（本机实际版本记于 out），仅平台通用 CPU 验证。

- `python -B -m unittest discover -s tests -p 'demo_differential_oracle_tests.py'`：**10/10**。覆盖 schema 所有 predicate、双侧同错、重复/顺序/负载/key 变化、未执行动作、未来 unchanged、bool/number 与 Unicode 精确语义、owner idle/deadline 和不可信 expected 拒绝。
- 同命令分别选 `demo_differential_observation_tests.py`、`demo_differential_protocol_tests.py`：**9/9、5/5**。包含零退出但缺 hello/receipt/图/complete、旧 source/nonce、错 DPR/fence/revision、callback 重写、校验和合法但 pixel stream 无效的 PNG，以及请求 17 个动作仅执行 8 个的负例。
- OpenSpec 2.0.0 doctor healthy、全 strict **109/0**、diff check 通过；只新增关闭 **4.3**，109 **15/61**。其他 change 的 checkbox 向量不变。

本阶段交付有限 oracle evaluator、保序 trace projection 和判定前的严格 receipt reader；原始日志/程序生成图仅留 ignored out。没有运行新的 browser/native GPU 批次，没有 paired、校准 profile、完整 runner 或真实 fault 红绿通过声明。3.4–3.6、其余 4.x 及平台项继续开放。
