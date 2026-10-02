# 平台通用实施与验证

2026-10-02，实际执行环境为 Windows/MSVC 14.51.36231、Ninja Multi-Config，使用 `windows-msvc-headless` configure preset 与对应 Debug/Release build/test presets。以下为平台通用合同；不代表 Linux 或原生 GPU 验收。

## 实现结果

- Effect GPU instance、packing 与 shader reference 移至 `renderer/common/rounded_effect_packing.*`；graphics 仅保留逻辑 Effect 数据与 CPU 几何/覆盖计算。Quad/Glyph/Effect 共用独立 `SceneDeviceMetrics`，拒绝非法 scale、零像素尺寸及不可表示的逻辑 viewport。
- Effect 的任何同步异常都会清除 metrics 缓存；写入 buffer 后失败/抛异常再回到原 metrics、且 CPU 无 dirty 时，仍完整重打包并上传。abandon 清空 GPU attachment，保留 staging capacity 与 CPU Effect，下一代创建新 buffer；不释放旧代际失效 handle。
- Core include/link 守卫拒绝 renderer 反向依赖，包括相对路径、条件 generator expression 与 alias；允许 renderer/common 依赖 Core、Recording 依赖 common。实际 compile_commands/Ninja 图确认 Effect packing 只属于 renderer/common，headless 不包含 SDL、平台字体与 shadercross。
- shader ABI v1、逻辑 scene v2 保持不变；Effect 为 112 bytes、alignment 16，七个字段偏移为 0/16/32/48/64/80/96，vertex count 为 6。shader 源码未改动。

## 验证结果

- Debug 全量 CTest：33/33，81.28 秒；后续增加 inset coverage 和 invalid-metrics-before-begin 断言，再构建并运行 5 项相关测试：5/5，1.54 秒。
- Release 全量 CTest：33/33，11.09 秒，包含上述最终断言。
- literal ABI、outer/inset/outline coverage、clip、alpha、DPI 1/1.25/1.5/2、三类 primitive 的统一 metrics、失效输入、局部/idle/zero/增长、写后失败恢复、abandon 重建及依赖正负 fixtures 均通过。
- Release Input 场景预热后，256 个 Input 分别执行 20,000 次 selection 与 composition-selection 更新：两者均 `allocations=0`，`dispatch_allocations=0`，`sync_allocations=0`；实测耗时 2100/2114 ms。Effect allocation 合同也通过。以上为 CPU/headless 数据，不是 GPU 时间测量。
- `openspec doctor --json` healthy；全量 strict 37 passed、0 failed；working/staged diff check 通过。

原始 CTest 输出见同目录 `common-debug-ctest.txt`、`common-debug-final-checks.txt`、`common-release-ctest.txt` 与 `common-release-details.txt`。Windows 原生验证单独记录；Linux 验收保持未完成。
