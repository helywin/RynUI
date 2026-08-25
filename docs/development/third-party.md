# 第三方依赖

RynUI 不强制绑定 vcpkg、Conan 或某个 Linux 发行版。CMake 通过 `RYNUI_DEPENDENCY_MODE` 显式选择依赖来源，只接受以下值：

- `BUNDLED`：默认值，下载并校验仓库锁定的 source archive。
- `SYSTEM`：只使用调用方提供的 CMake package，不允许隐式下载回退。

## 当前锁定

| 依赖 | 上游版本 | Source | SHA256 | License | 作用域 |
|---|---|---|---|---|---|
| SDL3 | `3.4.14` / `147a8ee32dbf9ac02f3794964490687b6bbda1bc` | `SDL3-3.4.14.tar.gz` | `30d4aa2b3037718142b32dffd4e72f917ebb6cc5227150e7bb9c45efb2153aeb` | Zlib | 平台与 GPU 后端 |
| SDL_shadercross | `e55cf5e31ced6f3d1be5cc6d0c50e99384f9f4ba` | commit archive | `342bb6a8e734745eb5951f25c87fa7aad62f46b3736def8681d9fa7ad046887f` | Zlib | 仅构建期 host tool |
| SPIRV-Cross | `1a6169566c73d3da552748fc372fe2bbb856e46e` | commit archive | `0f295b214b164e42a1d21537c8da7b44569806c16220dda9798558edfaacd11e` | Apache-2.0 OR MIT | SDL_shadercross 静态构建依赖 |
| DirectXShaderCompiler | `1.8.2502` | Windows/Linux x64 官方 binary archive | Windows: `70b1913a1bfce4a3e1a5311d16246f4ecdf3a3e613abec8aa529e57668426f85`; Linux: `e0580d90dbf6053a783ddd8d5153285f0606e5deaad17a7a6452f03acdf88c71` | NCSA + MIT + Microsoft Software License Terms | SDL_shadercross host tool 的 HLSL compiler |

完整 URL 与机器可读值保存在 `cmake/dependencies/RynUIDependencyLock.cmake`。SDL3 的 license 来自其锁定 release；SDL_shadercross 在锁定时没有正式 release 或 tag，因此使用官方仓库精确 commit，而不是把 `main` 当成版本。DXC binary archive 内含三份上游 license，故 lock 保留其组合说明而不伪装为单一 SPDX expression。

## BUNDLED 模式

```text
RYNUI_DEPENDENCY_MODE=BUNDLED
  -> FetchContent locked URL
  -> SHA256 validation
  -> SDL3::SDL3
  -> native host: shadercross CLI
       -> locked SPIRV-Cross static libraries
       -> locked official DXC binary archive
```

离线构建使用 CMake 标准 source override，不需要修改 lock：

```powershell
cmake --preset windows-msvc `
  -DFETCHCONTENT_SOURCE_DIR_SDL3=D:/deps/SDL `
  -DFETCHCONTENT_SOURCE_DIR_RYNUI_SDL_SHADERCROSS=D:/deps/SDL_shadercross `
  -DFETCHCONTENT_SOURCE_DIR_RYNUI_SPIRV_CROSS=D:/deps/SPIRV-Cross `
  -DFETCHCONTENT_SOURCE_DIR_RYNUI_DXC=D:/deps/dxc
```

本地源码必须与 lock 记录的版本相符；source override 由开发者负责准备，CMake 不会下载或更新该目录。

## SYSTEM 模式

vcpkg、Conan、发行版 package 或已有 superbuild 应提供兼容的 SDL3 CMake config package。RynUI 只接受官方规范 target `SDL3::SDL3`，并要求显式指定可在 host 上运行的 shadercross：

```powershell
cmake --preset windows-msvc-system `
  -DSDL3_ROOT=D:/deps/SDL3 `
  -DRYNUI_SHADERCROSS_EXECUTABLE=D:/tools/shadercross.exe
cmake --build --preset windows-msvc-system-debug
ctest --preset windows-msvc-system-debug
```

缺少 package、版本过低或没有规范 target 时 configure 会失败，不会切换到 BUNDLED。

## Shader 工具边界

单一 HLSL source 在 build tree 中离线生成 DXIL 与 SPIR-V。原生 `BUNDLED` 构建只为当前 host 构建 `shadercross` CLI；`SYSTEM` 与所有交叉编译必须通过 `RYNUI_SHADERCROSS_EXECUTABLE` 提供可运行的 host tool，configure 会先执行 `--help` 探测。交叉编译绝不执行 target binary。

`SDL_shadercross`、SPIRV-Cross 和 DXC 都只存在于构建工具链。RynUI library 与示例应用不链接 `SDL3_shadercross` runtime library，也不分发 DXC binary；如在其他产品流程中复制这些上游二进制，发布方必须单独复核 archive 内 license terms。
