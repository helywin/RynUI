# Tasks

## 1. 规划与基线（平台通用）

- [x] 1.1 完成 proposal/spec/design/tasks，确认逻辑 scene 与 packed ABI 分离；运行 `openspec doctor --json`、`openspec validate --all --strict --no-interactive`、`git diff --check`，记录 Windows 执行证据并提交规划。

## 2. 连贯 scene 迁移（平台通用）

- [ ] 2.1 实现独立 packed 类型与共同转换函数、迁移 logical producers，移除 Core GPU 同步入口并迁移所有 renderer/examples/tests；验证 literal 坐标、圆角、裁剪、glyph density/phase、retained store 与 dirty binding 测试。
- [ ] 2.2 实现 metrics 缓存、局部 pack/upload、首次/增长/resize 全量打包及失败恢复；在 Recording 检查 owned bytes、idle、partial、无 CPU dirty resize、upload/commit retry、无效 metrics 与 epoch reset，现有 scene backend/resource 测试通过。
- [ ] 2.3 更新 renderer contract、architecture、AGENTS 与研究状态入口，明确 CPU logical scene v2/GPU ABI v1 和后续 backend 规则；运行 headless Debug CTest、文档校验与 diff check，记录实际 Windows preset 后提交本阶段。

## 3. 集成收口（平台通用）

- [ ] 3.1 在 Windows 的 `windows-msvc-headless-release` 构建并运行完整 headless CTest；在 native Debug 构建中运行现有组件、资源和 interaction/allocation benchmark 验收，确认新 staging 不引入热路径分配回归；将结果记录于本 change evidence，校验文档后提交。

## 4. Windows 原生验收

- [ ] 4.1 使用 `windows-msvc-debug`、`windows-msvc-release` 构建全部目标，分别运行真实 Gallery smoke resize 与 Typography acceptance（系统 scale 和 override 2.0），检查剪裁/字形/装饰/Selection 视觉与退出状态；运行受影响 Windows 平台/GPU/lifetime 集成测试，保存本 change 独立 evidence 后提交。

## 5. Linux 原生验收

- [ ] 5.1 在实际 Linux 机器使用 GCC 或 Clang native preset 构建 Debug/Release，执行受影响 GPU/lifetime 集成测试及真实 Gallery resize/Typography density 验收并独立提交 Linux evidence；不得由 Windows/headless 结果代替。
