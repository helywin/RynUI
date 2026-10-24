# Tasks

## 1. 规划与合同

- [x] 1.1 核对锁定 6.6.5 Empty/Result 源码，完成 proposal/spec/design/tasks，通过 doctor、严格验证和 diff check 后本地提交。

## 2. 平台通用 API 与 Theme

- [x] 2.1 添加独立公开 typed API、Empty/Result Theme 默认/暗色/紧凑/继承/override/equality/hash/dirty，加入有意义的 public API/theme 测试；在独立 windows-msvc-headless MSVC Ninja Multi-Config 构建树以最多 4 jobs 构建并通过 Debug/Release 相关测试，从新 executable 生成五份 golden 并证明旧字段除 identity 外保持，再格式检查和本地提交。

## 3. 平台通用 retained 实现

- [x] 3.1 实现固定 slot 身份、reactive 标签/状态/主题、有限宽度居中换行、短/零约束裁剪与原生 extra 操作；使用实际 node/text/glyph/surface、Button 激活、Input 编辑保持与释放测试，在同一 MSVC headless 构建树通过 Debug/Release 后格式检查和本地提交。

## 4. 平台通用收口

- [x] 4.1 更新独立组件说明与 073 验收证据，运行 MSVC headless Debug/Release 完整 CTest、format check、doctor、strict validate 和 diff check；记录实际平台、命令、结果与未验证边界后本地提交。

- [x] 4.2 在 main 4efa310 上复现 Astra R3 的 Empty/Result 三处字体小尺寸发布后失败；局部修复字体范围合同，覆盖初始/响应式非法值、舍入阈值、真实 Text/glyph 原子性、DPI 切换及恢复，保留既有图示零尺寸语义；通过 MSVC headless Debug/Release 专项与相关共享回归、golden 保持检查和 format/doctor/strict/diff 后本地提交证据。

- [x] 4.3 实跑复现 Astra R4 的父算法变化导致现有继承子 Theme 字号失效；在发布父配置/snapshot/changed identities 或通知前预验证受影响的现有继承子树，覆盖直接/间接 Empty/Result、兄弟通知、同配置重试、合法恢复、inherit=false 与失效 weak child；运行 Debug/Release Theme/组件及零分配回归、格式与 OpenSpec 检查，记录实际失败/成功证据。

本 change 的完成范围是平台通用静态展示。真实 GPU、Linux、系统 IME 和上游插画像素等价不属于本次验收，不作为未完成 checkbox，也不宣称已验证。仅本地提交，不 fetch/rebase/push、不创建 PR、不 archive。

2026-10-24：1–3 已验收；Release 完整 142/142、Debug 073 专项/Theme algorithm/Switch features 5/5。4.1 因共享 Debug 的 Button/Popconfirm Theme 栈溢出及既有 Popconfirm vector copy 分配保持未勾选，详见 [验收与交接](evidence/platform-common-msvc-headless.md)，由父任务集成修复后收口。

2026-10-24 后续收口：已合入 committed main c3190b0，保留共享 24 个和本 change 2 个 Theme IIFE；增加 Popconfirm 空集合 guard 并实际证明两种 20,000 次更新均为 0 dispatch / 0 sync allocations。Astra R1 两项 P2 先复现后修复，最终源代码 28bee9e 的 MSVC headless Debug/Release 完整测试均 148/148，五份 fresh golden 双基线旧字段保持，format/doctor/strict/diff 全通过，故 4.1 现勾选；详见 [集成收口](evidence/integration-c3190b0/closure.md)。原始成功与失败证据保持不变。

2026-10-25 R3：本地 ff-only 到 4efa310 后，实际复现三处 `.49 dp` 字体更新发布后失败；仅增加 6 行局部 Theme 范围验证，要求字号至少 .5 dp。66 个非法配置拒绝与 3 个阈值/DPI 恢复 case、Debug/Release 12/12 专项通过，fresh goldens 对 4efa310 含 identity 全部保持，format/doctor/strict/diff 通过；详见 [R3 验收](evidence/review-r3-4efa310/acceptance.md)。共享 Text 未改，旧 148 全量结果保留而不重复宣称本阶段重跑。
