# 离屏文本实现化基线与验收

规划基线复用 028 的 `../028-20260925-coalesce-gallery-atlas-transfers/gallery-texture-chunk-after.csv`：Windows 11 10.0.26200 / Intel Core Ultra 9 285HX，正式 `windows-msvc` / Ninja Multi-Config / MSVC Release，五个独立进程实际 SDL D3D12 / DXIL，1280×900 logical，display scale 1.25。首帧 CPU 中位数 189,322 µs、首帧资源同步 5,294 µs、首帧 glyph 纹理区域 1,077；各进程固定 240 步滚动成功，后续帧平均 CPU 中位数 4,156 µs。这些是代码变更前的测量，不是本 change 的效果。

规划校验：`openspec validate 029-20260926-defer-offscreen-text-realization --strict --no-interactive` 通过；全仓 strict 为 23/29，旧 change 013、015、016、017、018、021 失败；`openspec doctor --json` 在当前 CLI 报 `unknown command 'doctor'`；`git diff --check` 通过。
