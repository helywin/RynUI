include_guard(GLOBAL)

set(RYNUI_SDL3_VERSION "3.4.14")
set(RYNUI_SDL3_COMMIT "147a8ee32dbf9ac02f3794964490687b6bbda1bc")
set(RYNUI_SDL3_SOURCE_URL
    "https://github.com/libsdl-org/SDL/releases/download/release-3.4.14/SDL3-3.4.14.tar.gz")
set(RYNUI_SDL3_SOURCE_SHA256
    "30d4aa2b3037718142b32dffd4e72f917ebb6cc5227150e7bb9c45efb2153aeb")
set(RYNUI_SDL3_LICENSE "Zlib")

# SDL_shadercross had no release or tag when this snapshot was locked, so the
# archive is tied to an exact upstream commit rather than a moving branch.
set(RYNUI_SDL_SHADERCROSS_COMMIT "e55cf5e31ced6f3d1be5cc6d0c50e99384f9f4ba")
set(RYNUI_SDL_SHADERCROSS_SOURCE_URL
    "https://github.com/libsdl-org/SDL_shadercross/archive/e55cf5e31ced6f3d1be5cc6d0c50e99384f9f4ba.tar.gz")
set(RYNUI_SDL_SHADERCROSS_SOURCE_SHA256
    "342bb6a8e734745eb5951f25c87fa7aad62f46b3736def8681d9fa7ad046887f")
set(RYNUI_SDL_SHADERCROSS_LICENSE "Zlib")

set(RYNUI_SPIRV_CROSS_COMMIT "1a6169566c73d3da552748fc372fe2bbb856e46e")
set(RYNUI_SPIRV_CROSS_SOURCE_URL
    "https://github.com/KhronosGroup/SPIRV-Cross/archive/1a6169566c73d3da552748fc372fe2bbb856e46e.tar.gz")
set(RYNUI_SPIRV_CROSS_SOURCE_SHA256
    "0f295b214b164e42a1d21537c8da7b44569806c16220dda9798558edfaacd11e")
set(RYNUI_SPIRV_CROSS_LICENSE "Apache-2.0 OR MIT")

set(RYNUI_DXC_VERSION "1.8.2502")
set(RYNUI_DXC_WINDOWS_X64_URL
    "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2502/dxc_2025_02_20.zip")
set(RYNUI_DXC_WINDOWS_X64_SHA256
    "70b1913a1bfce4a3e1a5311d16246f4ecdf3a3e613abec8aa529e57668426f85")
set(RYNUI_DXC_LINUX_X64_URL
    "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2502/linux_dxc_2025_02_20.x86_64.tar.gz")
set(RYNUI_DXC_LINUX_X64_SHA256
    "e0580d90dbf6053a783ddd8d5153285f0606e5deaad17a7a6452f03acdf88c71")
set(RYNUI_DXC_LICENSE "NCSA + MIT + Microsoft Software License Terms")
