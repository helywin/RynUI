# 077 平台通用实施证据

## 环境与边界

2026-10-25，Windows x64，MSVC 19.51.36256.0，CMake `windows-msvc-headless` preset、Ninja Multi-Config，Debug/Release。独立工作树从 `0bc5c97b99520549576b2d9028c943f2450d69a2` 开始，构建目录为本树 `out/build/windows-msvc-headless`，依赖源码和构建目录均为本树 `out/deps`，未复用其他工作树二进制。所有 build 使用 `--parallel 4`。

## 阶段 1

实际运行：

```text
cmake --preset windows-msvc-headless -DFETCHCONTENT_BASE_DIR=<this-worktree>/out/deps
cmake --build --preset windows-msvc-headless-debug --target rynui_date_picker_model_tests rynui_date_picker_public_api_tests --parallel 4
ctest --preset windows-msvc-headless-debug -R "rynui.date_picker_(model|public_api)$" --output-on-failure
cmake --build --preset windows-msvc-headless-release --target rynui_date_picker_model_tests rynui_date_picker_public_api_tests --parallel 4
ctest --preset windows-msvc-headless-release -R "rynui.date_picker_(model|public_api)$" --output-on-failure
```

Debug/Release 各 2/2 通过。覆盖严格 ISO 往返/非法格式、1900/2000/2100 世纪闰年、年 1/9999 边界和空白格、42 格跨月日期、月份/年份及日导航、range clamp、禁用提交、清空、受控拒绝与回写。全禁用谓词不会驱动导航搜索；public API smoke 验证 reactive Props、ref 和 typed footer。

`python scripts/format-code.py --check` 使用 clang-format 22.1.3：588 个自有源文件通过；OpenSpec doctor healthy，strict validation 76/76，`git diff --check` 通过。

本证据为平台通用 headless 合同，不能证明 GPU、原生窗口/DPI、Linux 或系统 IME 行为。
