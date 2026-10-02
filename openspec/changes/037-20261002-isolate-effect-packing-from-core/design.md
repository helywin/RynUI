# Design

## Context

动机见 proposal.md。035/036 的共同事务与 Quad/Glyph staging 已存在；Effect 的资源 helper 已在 common，但 packed 类型、metrics 和 reference 仍编译入 graphics。include guard 对 Core 与 common 使用相同例外，portable link closure 也只排除 native targets。

## Goals / Non-Goals

Goals：完成三类 primitive 的 renderer ownership；合法 metrics 下 packed ABI v1 与 CPU logical scene v2 保持；将 Core→renderer 依赖在 configure 阶段拒绝；补齐独立 Effect helper 的失败与 abandon 合同。

Non-Goals：不改 logical Effect store/culling/coverage、shader 数学、公开组件 API、事务接受语义、宿主生命周期和新平台支持。zero-extent surface 仍由宿主决定何时恢复，不传给 packing。

## Decisions

### 1. Common owns packed Effect and neutral metrics

迁移 `graphics/rounded_effect_gpu.*` 为 `renderer/common/rounded_effect_packing.*`，使用 `ryn::detail`，保留 112-byte/16-byte alignment 与 shader reference。`scene_metrics.*` 定义 SceneDeviceMetrics、validate_scene_device_metrics、scene_logical_viewport；各 primitive 与 SceneCpuData 直接使用它，移除旧 Core GPU 接口，不保留回指 alias。

备选方案是仍在 graphics 导出 alias 或把 Effect 塞入现有 Quad/Glyph header；前者继续使 Core 拥有 shader ABI，后者扩大所有消费者头依赖，均不采用。合法输入数学维持，新增 derived viewport 有限/正检查，避免极小 scale 导致无穷 viewport。

### 2. Guard dependency direction in addition to portability

按 source area 区分 Core 与 renderer/common：Core 不允许任何 renderer include，共同层只允许共同 renderer includes。增加 Core link closure，检查直接、间接和条件表达式中的 renderer target；原 portable closure 对 common/Recording 仍只排除具体 backend/OS。扩展现有配置 fixture，包含 Core include/link negative、common include/link positive；检查实际 compile graph，graphics 不再编译 GPU packing。

仅扫描已知 component 头不能阻止未来 input/text/graphics 回耦，故使用现有全部 Core area/target 集合；仍保留现有直接依赖断言。

### 3. Effect retry and epoch cache

独立 Effect synchronize 的异常路径统一 reset metrics；创建/上传候选 buffer 的失败仍释放候选并保留当前 buffer。失败后再次使用原 metrics 必须 full upload，即使 CPU dirty 已为空。abandon 清空 handle、容量、instance count、metrics，保留 staging capacity；不调用 release。与 SceneResources 的事务 rollback 和 epoch 重建保持兼容。

普通 material/geometry dirty 范围、metrics full upload、zero/idle 路径继续保留；不增加每帧整场景临时分配。以已有 allocation benchmark 和 API-owned bytes 验证。

## Risks / Trade-offs

- [迁移遗漏造成 ABI 或符号混用] → 全部 consumers 迁移、literal/coverage 与 shader contract 检查；shader 文件不改。
- [更强 guard 误伤 renderer 合法依赖] → 正负 configure fixture、实际 HEADLESS Debug/Release 构建与 compile graph 检查。
- [失败已写 GPU bytes，原 metrics idle 跳过] → 注入写后失败/异常，清空 CPU dirty 后回原 metrics，检查完整 owned bytes 恢复。
- [abandon 复用旧容量、upload 空 handle] → 新 epoch 下验证新 buffer 创建、不 release 旧 handle、CPU scene 保留。
- [本机无 Linux GPU/字体证据] → Linux native 单列未完成；不重复要求平台通用测试。

## Migration Plan

先校验规划并以 `docs:` 提交，再直接进入 apply。一次连贯迁移 source/header/namespace/metrics 和所有 consumers；实现 guard/cache，运行本机 Windows/MSVC 的 Ninja Multi-Config `windows-msvc-headless` Debug/Release 完整 CTest（含 Effect math/store/packing/resources 与 allocation），更新正式文档并提交。随后 native Debug/Release 构建、受影响 tests 和真实窗口 resize/Typography/Selection 验收，独立提交 Windows evidence；Linux 原生由实际机器独立完成。按阶段 commit 回退，不能混用旧 Core GPU 接口和新 renderer metrics。
