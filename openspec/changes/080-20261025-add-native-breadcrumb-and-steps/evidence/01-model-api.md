# Typed API 与状态模型

基线 `d89eac780eb4eb981813a5704dd51e7e79d4b2a9`；本 worktree 使用 Windows、MSVC 19.51、`Ninja Multi-Config` 和 `windows-msvc-headless`，每次 build `--parallel 4`。

- `cmake --preset windows-msvc-headless`：通过，HEADLESS/RECORDING。
- `cmake --build --preset windows-msvc-headless-debug --target rynui_navigation_model_tests --parallel 4`：通过。
- `cmake --build --preset windows-msvc-headless-release --target rynui_navigation_model_tests --parallel 4`：通过。
- `ctest --preset windows-msvc-headless-debug -R '^rynui.navigation_model$' --output-on-failure`：1/1。
- `ctest --preset windows-msvc-headless-release -R '^rynui.navigation_model$' --output-on-failure`：1/1。
- clang-format 22.1.3 check：613 files，0 failures；doctor healthy；strict 80/80；diff check 通过。

验证固定非空唯一 key、空/隐藏投影、原始索引、`size_t::max` current、状态覆盖、受控拒绝后重试、禁用恢复及 typed slots/Props 可编译。此阶段只验证 API/model，尚不代表 host 可运行交付。
