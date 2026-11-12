# 独立语义 oracle 与执行完整性

case v1 的有限 predicate 由正式 API/固定官方合同编写，分别作用于 Web 和 native 的实际 observation；`origin` 仅允许 `independent-contract` 或明确声明的 `web-observation`。expected 不能从 native observed output 构造。schema 拒绝 native expected 路径；evaluator 拒绝缺 oracle、错误来源及错误 adapter side。两侧结果相等不代替各自通过独立断言。

`oracle.evaluate` 支持 case schema 中全部 predicate：精确值、先前值不变、次数/顺序、因果先后、焦点、有序 keys、领域区间、输入消费、owner 退役和被动 idle。业务值不全局 round、不做 Unicode NFC，也不把 bool 当 number。路径使用严格 JSON Pointer；`unchanged` 只能引用已声明且更早执行的 action。缺字段、缺 action 或错误类型是失败，不能作为空值/零计数通过。

callback `count` 匹配 step/target/event；receipt `count` 的 event 为 action kind，另匹配实际 dispatch/hit。`ordered_events` 对 callbacks 保留事件顺序，对 receipts 保留 step 顺序。`happens_before` 要求第一类事件全部早于第二类事件，双方至少各一次。idle 的每个 owner-local counter 必须是实际整数零且时间达到要求；合法 shell owner 和共享缓存不属于该 owner，不能据其全局数量判泄漏。

`projected_trace` 只移除两侧不可比较的 adapter-local 时间、owner generation 和输入层名称；step、语义 target、稳定 key、event 和完整 payload 必须保留，包含 proposed/committed/accepted。重复事件不得去重。DOM/Core 不可对应的底层事件不得加入共同 callback stream 冒充同一合同。

`observations.read_observations` 在判定之前检查 hello→每个 action receipt→checkpoint→complete 的完整有序身份集合、本次 nonce/adapter、callback append-only sequence、target 集合、source/binary 闭包、实际 DPR/extent、capture hash/scene revision/fence 和完整图像。即使旧 finite runner 或子进程返回 0，漏执行动作、漏图或漏 complete 也不能通过。Listy 的 17 个请求动作只执行旧上限 8 个是明确负控制。

对应 CPU tests：`demo_differential_oracle_tests.py`、`demo_differential_observation_tests.py` 与原 `demo_differential_protocol_tests.py`。这些验证判定与协议，不代替真实 Web/native 执行、字体/color 校准或 GPU fault 红绿。Input 的 UTF-8 byte/DOM UTF-16 映射见 native 接口文档；定义迁移必须保留旧 case/red，不能算同条件产品修复。
