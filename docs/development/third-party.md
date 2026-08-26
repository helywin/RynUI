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
| FreeType | `2.14.3` / `0a0221a1347e2f1e07c395263540026e9a0aa7c7` | 官方 stable source archive | `36bc4f1cc413335368ee656c42afca65c5a3987e8768cc28cf11ba775e785a5f` | FTL OR GPL-2.0-only | 字体读取、度量与灰度 rasterization |
| HarfBuzz | `14.3.1` / `ab5ecbb83985034a76214ac0b2b833dcd590d774` | 官方 release source archive | `9dae9538aae2ffdf70cec31f2c27bf68e2aaeeae3112688467697d5faf6194f7` | MIT | UTF-8 shaping 与 FreeType bridge |
| Noto Sans | `2.008` / `ffebf8c1ee449e544955a7e813c54f9b73848eac` | 锁定 upstream font file | `b85c38ecea8a7cfb39c24e395a4007474fa5a4fc864f6ee33309eb4948d232d5` | OFL-1.1 | Latin fallback 验收 fixture |
| Noto Sans CJK SC | `2.004` / `523d033d6cb47f4a80c58a35753646f5c3608a78` | 锁定 upstream font file | `2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b` | OFL-1.1 | 简体中文 fallback 验收 fixture |

完整 URL 与机器可读值保存在 `cmake/dependencies/RynUIDependencyLock.cmake`。SDL3 的 license 来自其锁定 release；SDL_shadercross 在锁定时没有正式 release 或 tag，因此使用官方仓库精确 commit，而不是把 `main` 当成版本。DXC binary archive 内含三份上游 license，故 lock 保留其组合说明而不伪装为单一 SPDX expression。FreeType、HarfBuzz 与验收字体的仓库内 license 记录位于 `third_party/licenses/`；字体二进制只下载到 build tree，不进入 Git。

## BUNDLED 模式

```text
RYNUI_DEPENDENCY_MODE=BUNDLED
  -> FetchContent locked URL
  -> SHA256 validation
  -> SDL3::SDL3
  -> RynUI::FreeType
  -> RynUI::HarfBuzz
  -> build-tree Noto Sans / Noto Sans CJK SC validation fonts
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
  -DFETCHCONTENT_SOURCE_DIR_RYNUI_DXC=D:/deps/dxc `
  -DFETCHCONTENT_SOURCE_DIR_RYNUI_FREETYPE=D:/deps/freetype `
  -DFETCHCONTENT_SOURCE_DIR_RYNUI_HARFBUZZ=D:/deps/harfbuzz
```

本地源码必须与 lock 记录的版本相符；source override 由开发者负责准备，CMake 不会下载或更新该目录。

## SYSTEM 模式

vcpkg、Conan、发行版 package 或已有 superbuild 应提供版本匹配的 SDL3、FreeType 与 HarfBuzz CMake config package。RynUI 只接受 `SDL3::SDL3`、`Freetype::Freetype` 与 `harfbuzz::harfbuzz`，内部归一为 `RynUI::FreeType`/`RynUI::HarfBuzz`；同时必须显式指定可在 host 上运行的 shadercross 与两份验收字体：

```powershell
cmake --preset windows-msvc-system `
  -DSDL3_ROOT=D:/deps/SDL3 `
  -DFreetype_ROOT=D:/deps/freetype `
  -Dharfbuzz_ROOT=D:/deps/harfbuzz `
  -DRYNUI_SHADERCROSS_EXECUTABLE=D:/tools/shadercross.exe `
  -DRYNUI_SYSTEM_LATIN_FONT_FILE=D:/fonts/NotoSans-Regular.ttf `
  -DRYNUI_SYSTEM_CJK_FONT_FILE=D:/fonts/NotoSansCJKsc-Regular.otf
cmake --build --preset windows-msvc-system-debug
ctest --preset windows-msvc-system-debug
```

缺少 package、版本过低或没有规范 target 时 configure 会失败，不会切换到 BUNDLED。

## Shader 工具边界

单一 HLSL source 在 build tree 中离线生成 DXIL 与 SPIR-V。原生 `BUNDLED` 构建只为当前 host 构建 `shadercross` CLI；`SYSTEM` 与所有交叉编译必须通过 `RYNUI_SHADERCROSS_EXECUTABLE` 提供可运行的 host tool，configure 会先执行 `--help` 探测。交叉编译绝不执行 target binary。

`SDL_shadercross`、SPIRV-Cross 和 DXC 都只存在于构建工具链。RynUI library 与示例应用不链接 `SDL3_shadercross` runtime library，也不分发 DXC binary；如在其他产品流程中复制这些上游二进制，发布方必须单独复核 archive 内 license terms。
