# Linux patched Wayland dependencies 构建验证

- 日期：2026-08-28
- CMake：4.4.2
- generator：Ninja Multi-Config
- dependency mode：`BUNDLED`
- 语言标准：生成的 Debug Ninja 编译规则为 `-std=c++20`

## GCC preset

- preset：`linux-gcc`
- toolchain：GCC 16.2.1
- fresh configure：`cmake --fresh --preset linux-gcc`，退出码 0
- Debug build：`cmake --build --preset linux-gcc-debug`，退出码 0
- Release build：`cmake --build --preset linux-gcc-release`，退出码 0
- Debug 完整 CTest：138/138 passed
- Release 受影响 CTest：14/14 passed

受影响集合包括 libdecor/SDL patch source contract、bundled build graph、resize pacing、`BUNDLED|SYSTEM` resolver positive/negative fixtures、dependency lock、public API smoke 与 input dependency isolation。

`rynui_minimal` 的 ELF `DT_NEEDED` 包含 `libdecor-0.so.0`，`RUNPATH` 为：

```text
/home/jiang/code/RynUI/out/build/linux-gcc/_deps/rynui-libdecor-stage/lib
```

`ldd` 实际解析到：

```text
/home/jiang/code/RynUI/out/build/linux-gcc/_deps/rynui-libdecor-stage/lib/libdecor-0.so.0
```

构建合同同时验证 cairo plugin 位于同一 staging prefix 的 `lib/libdecor/plugins-1`。阶段 3 的原生 Wayland smoke 从该 staging prefix 加载 core/plugin，窗口正常关闭且程序退出码为 0。

## Clang preset

- preset：`linux-clang`
- C++ consumer：Clang 22.1.8
- 平台 C 依赖编译器：GCC 16.2.1（preset 的 `CMAKE_C_COMPILER=/usr/bin/cc`）
- fresh configure：`cmake --fresh --preset linux-clang`，退出码 0
- Debug build：`cmake --build --preset linux-clang-debug`，退出码 0
- Debug 受影响 CTest：14/14 passed

Clang build graph 中 SDL 私有编译定义包含 `SDL_LIBDECOR_HAS_RESIZING_STATE=1`，include path 指向 `out/build/linux-clang/_deps/rynui-libdecor-stage/include/libdecor-0`。Clang C++ consumer 成功链接由 build-local C ABI 依赖生成的 SDL/libdecor，公开头文件未出现 SDL 或 libdecor 类型。

## retained build tree 回归

验证期间发现 `cmake --fresh` 会重建顶层 cache，但可保留 FetchContent subbuild 的 patch stamp 与 source tree。依赖编排现于每次 SDL 源码 configure 之前显式执行 standalone patch 校验/应用；patch runner 使用强制 forward/reverse dry-run，避免 GNU patch 自动反向推断把 pristine source 误判为已应用。合同测试覆盖 pristine 首次应用、already-applied 幂等校验、严格重复应用失败和错误上下文 fail-fast。

最终状态下，GCC Debug 完整 CTest、GCC Release 受影响 CTest和 Clang Debug 受影响 CTest均通过；没有以 Linux 静态合同替代任务 5 的双输出真实窗口验收或任务 6 的 Windows/MSVC 验收。
