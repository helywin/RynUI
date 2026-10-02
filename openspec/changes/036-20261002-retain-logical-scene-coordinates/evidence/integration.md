# 集成验收

执行日期：2026-10-02。执行环境：Windows、MSVC 14.51.36231、Ninja Multi-Config。

## 平台通用验收

- `cmake --build --preset windows-msvc-headless-release` 成功。
- `ctest --preset windows-msvc-headless-release --output-on-failure`：27/27 通过，5.25 秒；原始日志见 [common-release-ctest.txt](common-release-ctest.txt)。Debug 的 27/27 见 [logical-scene.md](logical-scene.md)。
- `cmake --build --preset windows-msvc-debug` 成功；完整 CTest 241/241 通过，179.71 秒，见 [native-debug-ctest.txt](native-debug-ctest.txt)。其中组件、场景资源、interaction、Input scene allocation、Button spinner 与 rendering locality 等已有合同和 benchmark 全部通过。
- `rynui.input_scene_allocation` 的现有断言确认：256 个 Input 在预热后更新 selection/composition 并同步场景，计数范围内分配为零，且无额外 atlas/effect upload、无关节点 measure/place。此结果验证现有热路径的分配合同，不代表 GPU 时间测量。
- Recording 验证 resize 无 CPU dirty 时重打包、idle 零上传、局部更新、失败重试与 epoch reset；CPU logical 数据和字体资源保持不因 projection 改变。具体覆盖见 [logical-scene.md](logical-scene.md)。

上述通用逻辑在 Windows 完成一次。完整 native CTest 中的平台分支仅对本机 Windows 有效；Linux 的实际 GPU、窗口和字体验收仍属于任务 5.1。

## 文档与提交检查

`openspec doctor --json` healthy；`openspec validate --all --strict --no-interactive` 36/36；`git diff --check` 通过。
