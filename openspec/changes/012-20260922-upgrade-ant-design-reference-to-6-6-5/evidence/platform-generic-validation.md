# Ant Design 6.6.5 平台通用集成验收

- 执行环境：Windows 10.0.26200，`windows-msvc` / Ninja Multi-Config / Debug，MSVC x64 19.51.36256；通过 `scripts/build-windows.ps1 -Configuration Debug` 进入 Visual Studio Developer Environment、构建并运行 CTest。
- 完整 CTest：211/211 PASS，0 FAIL，总计 256.98 秒。覆盖 Theme、Button、Input、Gallery、Token、生成器、source/current-baseline/evidence contract、headless journey、benchmark、public compile-fail、依赖模式与锁、shader、Python cache clean 等。耗时的 `rynui.input_scene_allocation` 为 134.97 秒，通过。
- 官方来源离线复算：`python -B tools/audit_ant_design_665.py --old-source out/upstream-ant-design-6.5.0 --new-source out/upstream-ant-design-6.6.5 --check --self-test` PASS；249 个审查路径，Token 1194→1198，旧版目录 72 项与新版 Listy 候选一致。阶段 2 的 importer 补全 `focusOutline` 默认值后，账本相应从空值校正为上游 `true`，复算一致。
- `python -B tools/check_ant_design_current_baseline.py --self-test`、`python -B tools/verify_ant_design_665_evidence.py --self-test` 均 PASS；证据合同的正反 fixture 拒绝旧版、planning-only、错误 SHA、跨目录路径、错误 preset 和非零退出码。此时只有合同，没有声称真实窗口的 passed evidence。
- `openspec doctor --json` healthy；`openspec validate --all --strict --no-interactive` 12/12 PASS；`git diff --check` PASS。
- 此文件只记录平台通用逻辑在 Windows 上的执行，不代替阶段 7 的 Windows 真实窗口、DPI/GPU/字体截图验收，也不代替阶段 8 的原生 Linux Wayland 验收；不复用历史 6.5.0 截图。
