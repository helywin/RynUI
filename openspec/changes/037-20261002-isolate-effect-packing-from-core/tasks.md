# Tasks

## 1. 规划（平台通用）

- [x] 1.1 完成 proposal/spec/design/tasks；运行 doctor、全量 strict 与 diff check，保存规划证据并按英文 Conventional Commit 格式提交，然后直接 apply。

## 2. 连贯边界迁移与恢复（平台通用）

- [x] 2.1 迁移 Effect packing/reference、建立独立 SceneDeviceMetrics 并迁移全部 consumers；验证 Effect literal ABI/coverage、三类 primitive 共用 metrics、非法与不可表示 viewport 在 upload 前拒绝，graphics 不再编译 GPU packing。
- [x] 2.2 实现 Effect 异常 metrics 失效与 abandon 重建；注入写后失败/异常后回原 metrics、无 CPU dirty 完整重试、新代际 buffer 创建与 CPU effect 保留，现有局部/idle/zero/增长/clip 合同通过。
- [x] 2.3 强化 Core include/link 守卫并补正负 fixtures，更新 renderer contract、architecture、AGENTS 与研究入口；在 Windows/MSVC 使用 windows-msvc-headless Debug/Release 完整 CTest（补入 Effect math/store/scene/packing 与 allocation），验证实际 build graph、热路径分配合同及 doctor/strict/diff check，记录通用证据并提交本阶段。

## 3. Windows 原生验收

- [x] 3.1 使用 windows-msvc-debug/release 构建全部 native 目标；运行受影响 Effect、packing、SceneBackend、组件/资源/allocation、shader/font/lifetime tests，并执行两种配置的真实 Gallery resize、Typography（系统 scale 和 2.0）与 Selection acceptance；检查截图、退出状态与最终二进制哈希，记录独立 Windows evidence、doctor/strict/diff check 后提交。

## 4. Linux 原生验收

- [ ] 4.1 在实际 Linux 机器使用 native GCC 或 Clang preset 构建 Debug/Release，运行受影响 shader/font/lifetime 集成与真实 Gallery resize/Typography/Selection 验收，独立记录并提交 Linux evidence；通用合同无需重复，Windows 不替代本项。
