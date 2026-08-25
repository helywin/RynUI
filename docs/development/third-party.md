# 第三方依赖

RynUI 不强制绑定 vcpkg、Conan 或某个 Linux 发行版。CMake 通过 `RYNUI_DEPENDENCY_MODE` 显式选择依赖来源，只接受以下值：

- `BUNDLED`：默认值，下载并校验仓库锁定的 source archive。
- `SYSTEM`：只使用调用方提供的 CMake package，不允许隐式下载回退。

## 当前锁定

| 依赖 | 上游版本 | Source | SHA256 | License | 作用域 |
|---|---|---|---|---|---|
| SDL3 | `3.4.14` / `147a8ee32dbf9ac02f3794964490687b6bbda1bc` | `SDL3-3.4.14.tar.gz` | `30d4aa2b3037718142b32dffd4e72f917ebb6cc5227150e7bb9c45efb2153aeb` | Zlib | 平台与 GPU 后端 |
| SDL_shadercross | `e55cf5e31ced6f3d1be5cc6d0c50e99384f9f4ba` | commit archive | `342bb6a8e734745eb5951f25c87fa7aad62f46b3736def8681d9fa7ad046887f` | Zlib | 仅构建期 host tool |

完整 URL 与机器可读值保存在 `cmake/dependencies/RynUIDependencyLock.cmake`。SDL3 的 license 来自其锁定 release；SDL_shadercross 在锁定时没有正式 release 或 tag，因此使用官方仓库精确 commit，而不是把 `main` 当成版本。

## BUNDLED 模式

```text
RYNUI_DEPENDENCY_MODE=BUNDLED
  -> FetchContent locked URL
  -> SHA256 validation
  -> SDL3::SDL3
```

离线构建使用 CMake 标准 source override，不需要修改 lock：

```powershell
cmake --preset windows-msvc -DFETCHCONTENT_SOURCE_DIR_SDL3=D:/deps/SDL
```

本地源码必须与 lock 记录的版本相符；source override 由开发者负责准备，CMake 不会下载或更新该目录。

## SYSTEM 模式

vcpkg、Conan、发行版 package 或已有 superbuild 应提供兼容的 SDL3 CMake config package。RynUI 只接受官方规范 target `SDL3::SDL3`：

```powershell
cmake --preset windows-msvc-system -DSDL3_ROOT=D:/deps/SDL3
cmake --build --preset windows-msvc-system-debug
ctest --preset windows-msvc-system-debug
```

缺少 package、版本过低或没有规范 target 时 configure 会失败，不会切换到 BUNDLED。

## Shader 工具边界

`SDL_shadercross` 的 archive 在任务 2.1 中只进入依赖 lock；CLI 构建、`RYNUI_SHADERCROSS_EXECUTABLE` override 和 DXIL/SPIR-V 生成属于任务 2.2。应用运行时不得链接 SDL_shadercross 转换库。
